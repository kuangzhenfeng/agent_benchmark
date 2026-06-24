# Blind Scoring Package

## Scope

Each `Pxx` directory is one complete anonymous submission for the three
`benchmark-v1` questions. Treat every participant as independent. The
directory names and file contents have been checked for explicit agent/model
identifiers; do not attempt to infer identity from coding style.

Use the `QUESTION.md`, implementation, tests, and `ANSWER.md` within every
question directory. The private reference in the sibling `scorer-reference`
directory is a semantic calibration aid, not the only acceptable solution.

## Rubric (per question, 100 points)

| Dimension | Points |
|---|---:|
| Correctness | 45 |
| C++ quality | 20 |
| Constraint compliance | 15 |
| Maintainability | 10 |
| Verification evidence | 10 |

Apply the following caps where applicable:

- no core implementation: 30;
- fabricated tests, commands, or files: 40;
- evidence of reading another participant's result: 50;
- violation of a `QUESTION.md` hard-cap condition: 60;
- large irrelevant rewrite: maintainability 0;
- broken structure preventing review: 50.

## Required report format

For every `Pxx` and question, report the five dimension scores, total, and
concrete evidence tied to the question requirements, source code, and test or
verification record. Also provide totals/averages, a relative ranking, and
fairness limitations. Refer only to `Pxx` identifiers.
