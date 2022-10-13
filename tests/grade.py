#!/usr/bin/python3

import argparse
import json
import os.path
import sys

class Assignment(object):
    
    def __init__(self, name, points):
        self.name = name
        self.points = points
        self.categories = {}
        self.lcategories = []
        
    def add_category(self, cid, name, points):
        self.categories[cid] = (name, points)
        self.lcategories.append(cid)
    

ASSIGNMENT_1 = Assignment("Assignment 1", 50)    
ASSIGNMENT_2 = Assignment("Assignment 2", 50)    

ASSIGNMENTS = [ASSIGNMENT_1, ASSIGNMENT_2]

ASSIGNMENT_1.add_category("conn_init", "3-way handshake", 20)
ASSIGNMENT_1.add_category("conn_term", "Connection tear-down", 10)
ASSIGNMENT_1.add_category("data_transfer", "Data transfer", 20)

ASSIGNMENT_2.add_category("multitimer", "Timer API", 10)
ASSIGNMENT_2.add_category("unreliable_conn_init", "Retransmissions - 3-way handshake", 5)
ASSIGNMENT_2.add_category("unreliable_conn_term", "Retransmissions - Connection tear-down", 5)    
ASSIGNMENT_2.add_category("unreliable_data_transfer", "Retransmissions - Data transfer", 15)
ASSIGNMENT_2.add_category("persist", "Persist timer", 5)
ASSIGNMENT_2.add_category("unreliable_out_of_order", "Out-of-order delivery", 10)


parser = argparse.ArgumentParser()
parser.add_argument("--tests-file", default="{}/alltests".format(os.path.dirname(os.path.realpath(__file__))))
parser.add_argument("--report-file", default="results.json")
parser.add_argument("--csv", action="store_true")
parser.add_argument("--gradescope", action="store_true")
parser.add_argument("--gradescope-assignment", type=int)

args = parser.parse_args()

if not os.path.exists(args.tests_file):
    print("Tests file not found: {}".format(args.tests_file))
    sys.exit(1)

if not os.path.exists(args.report_file):
    print("Test results file not found: {}".format(args.report_file))
    print("Make sure you've run the tests before running this script.")
    sys.exit(1)


tests = {}
all_test_ids = set()

with open(args.tests_file) as f:
    for l in f:
        category, test_id = l.strip().split("::")
        tests.setdefault(category, {})[test_id] = 0
        all_test_ids.add(test_id)

run_test_ids = set()
with open(args.report_file) as f:
    results = json.load(f)

for test_suite in results["test_suites"]:
    category = test_suite["name"]
    
    for test in test_suite["tests"]:
        test_id = test["name"]
        run_test_ids.add(test_id)
        if test["status"] == "PASSED":
            tests[category][test_id] = 1
    
not_run = all_test_ids.difference(run_test_ids)

if args.gradescope:
    gradescope_json = {}
    gradescope_json["tests"] = []

if len(not_run) > 0 and not args.csv:
    print("WARNING: Missing results from {} tests (out of {} possible tests)\n".format(len(not_run), len(all_test_ids)))

    if args.gradescope:
        gradescope_json["output"] = "We were unable to run some or all of the tests due to an error in your code."

scores = {}
for assignment in ASSIGNMENTS:
    scores[assignment.name] = {}
    for category in assignment.lcategories:
        num_total = len(tests[category])
        num_success = sum(tests[category].values())
        num_failed = num_total - num_success
        scores[assignment.name][category] = (num_success, num_failed, num_total)

pscores = []
for i, assignment in enumerate(ASSIGNMENTS):
    pscore = 0.0
    if not args.csv and not args.gradescope:
        print(assignment.name)
        print("=" * 78)
        print("%-40s %-6s / %-10s  %-6s / %-10s" % ("Category", "Passed", "Total", "Score", "Points"))
        print("-" * 78)
    for cid in assignment.lcategories:
        (num_success, num_failed, num_total) = scores[assignment.name][cid]
        cname, cpoints = assignment.categories[cid]

        if num_total == 0:
            cscore = 0.0
        else:
            cscore = (float(num_success) / num_total) * cpoints

        pscore += cscore
        if not args.csv and not args.gradescope:
            print("%-40s %-6i / %-10i  %-6.2f / %-10.2f" % (cname, num_success, num_total, cscore, cpoints))
        elif args.gradescope and (args.gradescope_assignment is None or args.gradescope_assignment == i+1):
            gs_test = {}
            gs_test["score"] = cscore
            gs_test["max_score"] = cpoints
            gs_test["name"] = cname

            gradescope_json["tests"].append(gs_test)
    if not args.csv and not args.gradescope:
        print("-" * 78)
        print("%59s = %-6.2f / %-10i" % ("TOTAL", pscore, assignment.points))
        print("=" * 78)
        print()
    pscores.append(pscore)

    if args.gradescope and args.gradescope_assignment == i+1:
        gradescope_json["score"] = pscore
        gradescope_json["visibility"] = "visible"
        gradescope_json["stdout_visibility"] = "visible"

        print(json.dumps(gradescope_json, indent=2))

if args.gradescope and args.gradescope_assignment is None:
    gradescope_json["score"] = sum(pscores)
    gradescope_json["visibility"] = "visible"
    gradescope_json["stdout_visibility"] = "visible"

    print(json.dumps(gradescope_json, indent=2))


if args.csv:
    print(",".join([str(s) for s in pscores]))


