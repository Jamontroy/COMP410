# COMP 410 — Advanced Operating Systems
## Week 5 — Report

<!-- Replace the italic prompt under each heading with your own words. Keep the headings: the specification names them, and `make test` checks that they are here. -->

## Prediction

2.3 Pathology is predicted using the seed % 3 to give a 0,1,2 in workload.c. I don't you literally meant predict it but with my seed % 3 the output is 0 meaning it will run through the blocked holder pathology. In the data I expect to see a lot of processes going through quickly with the occasional long wait time. This will probably be exposed through a low average time for a worker but a high max time for a worker.
*Your prediction, before you have measured anything: which of the three pathologies you expect, why, and what you expect `make analyze` to show for it. Write this BEFORE your first `make bench`, and do not edit it afterward (handout 2.3).*

## Answers

*One line per key, `key: value`, the numbers copied as `make analyze` prints them. Write `not attempted` for the last five if you did not do section 3.*

derived_seed: <grep WL_SEED config/params.h>
runs_completed: <rows in bench/audit.csv -- at least 20>
bug_fraction_median: <from make analyze>
bug_fraction_spread: <from make analyze>
max_latency_median_us: <from make analyze>
shard_ratio_median: <from make analyze>
identified_pathology: <blocked-holder | burst-arrival | uneven-partition | not-resolved>
predicted_pathology: <what ## Prediction says, unchanged>
prediction_assessment: <correct | partially | wrong>
window_us_median: <from make analyze, or not attempted>
violation_fraction_median: <from make analyze, or not attempted>
violation_fraction_spread: <from make analyze, or not attempted>
episodes_median: <from make analyze, or not attempted>
invariant_violated: <yes | no | not-resolved | not attempted>

## What

*What you wrote, in two or three sentences.*
For section 2.1:
- I wrote the median_of method which used qsort on a seperate array to sort the values of v[], then pulled the median. 
- I wrote spread_of which simply iterated through v[] and compared every value to pull the maximum and minimum which were then subtracted to pull the spread.
- I wrote classify latency which took the BUG_MULTIPLIER and multiplied it with baseline_median_us and checked if they were less than latency_us. If true it returns 1, if false it returns 0.

For section 2.2:
- I wrote a test which found a bug within how my code uses qsort and compare double. The way it is set up it returns an integer value which could misrepresent the double value of v. It passes all 17 tests but by placing values of v close together 2.0, 1.2, 1.8. They are not sorted correctly and return the wrong median.


## Results

*Your median hand-off latency, the threshold it gives, and which tasks are bug-length (handout 2.5). Then which pathology you have, and the figures, as medians with spreads, that rule out each of the other two. Compare with your Prediction, honestly. For 3.0: your `make windows` output, the window you chose and why, whether your pool violates the invariant or only queues, and where the latency rule and the invariant disagree (handout 3.2 and 3.4).*

## Paper connection

*ONE measured result and ONE named claim in Lozi et al. (2016), with its section. Say whether your result supports it, differs, or does not settle it, and why: their bugs are multi-node phenomena on a 64-core, 8-node machine.*

## Limits

*What this hardware and this VM cannot show, and where your OWN data does not resolve the question. "My data does not separate these two", with the numbers that overlap, is a complete answer.*

## Citations

*Author-year for the assigned paper and anything else you drew on, including any technique not covered in Weeks 1–5. If you used an AI tool at any point, name it and say what for — the syllabus does not permit AI agents on submitted work, and saying so plainly is always better than leaving it out.*
