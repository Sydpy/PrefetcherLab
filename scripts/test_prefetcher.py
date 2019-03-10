#!/usr/bin/env python2
"""
Run the simulator and record statistics.
"""

import sys, os, os.path, glob

frameDir = os.environ['PREFETCHER_FRAMEWORK']
homeDir = os.path.realpath(os.path.dirname(os.path.realpath(__file__))+ '/..')

sys.path.append(frameDir)

DEBUG = int(os.getenv('DEBUG', "0"))

from lib.run_util import *
import lib.stats as stats

# Uncomment this to print commands instead of executing them.
#dry_run()


# Set paths
if DEBUG:
    m5_path(homeDir + '/build/ALPHA_SE/m5.debug')
    m5_args('--trace-flags=HWPrefetch', '--remote-gdb-port=0', '-re')
    if not os.path.exists(homeDir + '/build/ALPHA_SE/m5.debug'):
        print >>sys.stderr, "Could not find the M5 binary, run compile.sh to compile with your prefetcher."
        sys.exit(1)
    print "Remember to recompile after making changes."
else:
    m5_path(homeDir + '/build/ALPHA_SE/m5.opt')
    if not os.path.exists(homeDir + '/build/ALPHA_SE/m5.opt'):
        print >>sys.stderr, "Could not find the M5 binary, run compile.sh to compile with your prefetcher."
        sys.exit(1)
    print "Remember to recompile after making changes."

se_path(frameDir + '/m5/configs/example/se.py')

# Check that M5 is compiled

# Set output directory
global_prefix(homeDir + '/output/')

# Configure
global_args(
    '--checkpoint-dir=' + frameDir + '/lib/cp',
    '--checkpoint-restore=%d' % 1e9, '--at-instruction',
    '--caches', '--l2cache',
    '--standard-switch', '--warmup-insts=%d' % 1e7,
    '--max-inst=%d' % 1e7,
    '--l2size=1MB',
    '--membus-width=8', '--membus-clock=400MHz', '--mem-latency=30ns',
)

# Prefetchers to run
prefetchers = Config('user', ['--prefetcher=on_access=true:policy=proxy'])

# Tests to run
tests = spec_configs
if DEBUG:
    tests = spec_configs[:1]

configs = cross(tests, prefetchers)

# Run tests
os.chdir(homeDir)
os.environ['M5_CPU2000'] = homeDir + '/data/cpu2000'
run_configs(configs)

# Read statistics
stats.BASELINE_PF = 'none'
pf_stats = stats.read_stats(*glob.glob(frameDir + '/lib/stats/*_1e7'))

pf_stats.update(stats.build_stats(homeDir + '/output'))

# Write statistics
stats_file = open(homeDir + '/stats.txt', 'w')
def save_stats(pf, test, echo):
    table = stats.format_stats(pf_stats, pf, test)
    stats_file.write(table)
    if echo:
        print table

# Prefetcher comparison for each test
for test in sorted(pf_stats['user']):
    save_stats('all', test, False)
# User prefetcher results.
save_stats('user', 'all', True)
# Summary
save_stats('all', 'all', True)

stats_file.close()
