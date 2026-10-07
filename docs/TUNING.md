# Tuning and forking guide

CodeQuizzio is built so the "feel" of the game can be changed without touching C++, and so the whole thing can be reused for a different subject (another language, vocabulary, shortcuts, anything you drill by typing).

- **What you see next** is controlled by [`config/selector.json`](../config/selector.json), documented below. No recompiling.
- **How points work** lives in [`core/Scoring.cpp`](../core/Scoring.cpp), as named constants at the top of the file.
- **The questions** are plain JSON in [`questions/`](../questions).
- **The whole algorithm** can be replaced: it sits behind the small `Selector` interface in [`core/Selector.h`](../core/Selector.h).

## Changing the settings

Edit `config/selector.json` (the build copies it next to the app), or point the `CQ_SELECTOR_CONFIG` environment variable at your own file. You only need to list the settings you want to change; everything else keeps its default.

```json
{ "needFloor": 0.05, "forgetHalfLifeDays": 7 }
```

A mistake never silently does nothing: an unknown name, a wrong type, or an out-of-range value makes the game ignore the whole file, use the defaults, and log a warning that names the problem.

## How the next question is chosen

Each question gets a weight, and the next question is a weighted random draw. The weight is the product of a few independent factors, and each factor has its own settings:

| Factor | What it does | Needs history? |
|---|---|---|
| **Difficulty** | A bell curve around a moving *target difficulty*. The target rises after fast correct answers and falls after misses and skips. | No |
| **Need** | Questions you know poorly or slowly weigh more. Mastered questions weigh `needFloor`, so they still come back now and then. | Yes |
| **Topic** | Questions in your weakest tags get a boost. | Yes |
| **Session misses** | Questions you missed earlier this session get a boost. | No |
| **Unseen** | Questions you have never seen get a boost. | No |

Two other rules sit on top: nothing repeats within `recentWindow` questions, and a missed question returns after `requeueDelay` questions.

### How "mastery" is computed

For each question the game computes a mastery score from 0 (don't know it) to 1 (know it cold):

1. **Accuracy**: the share of correct answers over the last 8 attempts, with newer attempts counting more (`recencyDecay`). A skip counts as `skipPenalty` of a miss.
2. **Speed**: how your average correct time compares to the question's par time. This blends in according to `speedWeight`.
3. **Confidence**: with few attempts the score is pulled toward `masteryPrior`. After about `confidenceAttempts` attempts the evidence dominates. This stops one lucky answer from counting as mastery.
4. **Forgetting**: the score halves every `forgetHalfLifeDays` days since you last saw the question, so old knowledge comes back up for review.

*Need* is simply `1 - mastery`.

### Where you start

At the start of a session the target difficulty is set from your history: the game finds the highest difficulty level you have down (average mastery of at least `startMasteryThreshold`, judged from at least `startMinAttempts` attempted questions, with no gaps below it) and starts `startOffset` above it. With no history it starts at level 1.

## Settings reference

### Within a session

| Setting | Default | Range | Meaning |
|---|---|---|---|
| `recentWindow` | 5 | 0 to 100 | No question repeats within this many questions. Lower for small question banks. |
| `requeueDelay` | 3 | 1 to 100 | A missed question comes back after this many questions. |
| `sigma` | 1.5 | 0.1 to 20 | How tightly questions cluster around the target difficulty. Small = stays right at your level; large = wide mix of easy and hard. |
| `sessionMissWeight` | 0.5 | 0 to 10 | Extra weight per miss of a question in the current session. |
| `unseenBoost` | 1.5 | 0 to 20 | Weight multiplier for questions you have never seen. Raise it to see new material sooner. |
| `fastStep` | 0.3 | 0 to 5 | The target rises by this after a correct answer at or under par time. |
| `slowStep` | 0.1 | 0 to 5 | ...or by this after a correct answer slower than par. |
| `wrongStep` | -0.5 | -5 to 0 | The target changes by this after a wrong answer. |
| `skipStep` | -0.3 | -5 to 0 | The target changes by this after a skip. |

### Using saved history

| Setting | Default | Range | Meaning |
|---|---|---|---|
| `useHistory` | true | true / false | Master switch. `false` makes the game forget everything between sessions (selection only), as if there were no saved progress. |
| `masteryPrior` | 0.5 | 0 to 1 | Assumed mastery when there is no evidence yet. |
| `confidenceAttempts` | 3 | 0 to 100 | How many attempts before history outweighs the prior. `0` trusts even a single attempt completely. |
| `speedWeight` | 0.3 | 0 to 1 | How much answer speed counts toward mastery. `0` = only accuracy matters; `1` = speed matters as much as accuracy. |
| `recencyDecay` | 0.8 | 0.05 to 1 | Each older attempt counts this much less than the next newer one. `1` = all attempts count equally. |
| `skipPenalty` | 1.0 | 0 to 1 | A skip counts as this fraction of a miss. `0` = skips don't hurt mastery. |
| `forgetHalfLifeDays` | 14 | 0 to 3650 | Mastery halves after this many days unseen. `0` = never forget. |
| `needFloor` | 0.15 | 0 to 1 | Weight of a fully mastered question relative to one you don't know. `0` = mastered questions disappear; `1` = mastery has no effect. |
| `tagBoost` | 0.5 | 0 to 5 | Extra weight for questions in your weakest topics. `0` turns the topic boost off. |
| `startMasteryThreshold` | 0.7 | 0 to 1 | Average mastery a difficulty level needs to count as "comfortable". |
| `startMinAttempts` | 2 | 1 to 100 | Attempted questions needed at a level before it can be judged. |
| `startOffset` | 0.5 | 0 to 5 | Start this far above your comfortable level. |

## Recipes

- **Drill my weak spots harder:** raise `tagBoost` (try 2) and lower `needFloor` (try 0.05).
- **Keep things fresh, less repetition:** raise `unseenBoost` (try 4) and `recentWindow` (try 10).
- **Make speed matter more:** raise `speedWeight` (try 0.6).
- **Spaced repetition over days:** lower `forgetHalfLifeDays` (try 5). For cramming, raise it or set `0`.
- **Gentler difficulty ramp:** lower `fastStep` and `startOffset`.
- **Fixed difficulty band:** set `fastStep`, `slowStep`, `wrongStep` and `skipStep` all to `0`; the target stays where it starts. Lower `sigma` to stay close to it.
- **Ignore history completely:** `"useHistory": false`.

## Forking this for another subject

1. **Questions**: replace the files in `questions/` with your own. See the format in the main README. Use `tags` for topics, since the topic boost relies on them, and `difficulty` from 1 to 10.
2. **Feel**: adjust `config/selector.json` using the tables above.
3. **Scoring**: edit the constants at the top of `core/Scoring.cpp`. Par time currently assumes typing speed of about 3 characters per second plus reading time; for a different input style (shorter answers, different keyboards) change `kSecondsPerChar` and `kReadSeconds`.
4. **Answer matching**: `core/AnswerChecker.cpp` currently ignores whitespace around punctuation, which suits code. For prose or vocabulary you may want case-insensitive matching or a different normalization.
5. **Algorithm**: to replace the selection logic entirely, implement the `Selector` interface (`next` and `record`) and construct your class instead of `AdaptiveSelector` in `ui/QuizController.cpp`. Nothing else needs to change.
6. **Rename**: the app name appears in `CMakeLists.txt`, `ui/main.cpp`, `ui/qml/Main.qml`, and the progress folder name.
