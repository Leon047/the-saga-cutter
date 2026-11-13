---
name: fable-plan
description: Plan a feature or change with Fable, then hand the plan to cheaper models to implement — Sonnet for routine steps, Opus for tricky ones — and have Fable review the result against the plan. Writes the plan to Docs/Plans/<slug>.md as the contract between models, pauses for approval, runs independent steps in parallel, and verifies with a build or PIE check. Use when the user says "/fable-plan", "plan this with Fable and let Sonnet build it", "plan then implement", "cheap implement from a strong plan" or similar, for one feature done in one sitting (for multi-day unattended efforts use the multisession skill instead).
argument-hint: "<task description> [--impl sonnet|opus|auto] [--no-approve] [--no-review]"
---

# fable-plan

Strong model plans, cheaper models build, strong model reviews. The plan file is the only thing passed between
them, so it must stand on its own.

Arguments:
- `<task>` — what to build. Required; ask once if missing.
- `--impl sonnet|opus|auto` — implementer model. `auto` (default) uses each step's difficulty tag.
- `--no-approve` — skip the approval pause (only when the user asks for it).
- `--no-review` — skip the final Fable review.

**You (the session running this skill) are the orchestrator.** Subagents cannot spawn subagents, so every
planner, implementer and reviewer is started from here with the Agent tool. Never delegate the whole pipeline to
one agent.

## 1. Plan (Fable)

Pick a slug (kebab-case, ≤5 words) and the path `Docs/Plans/<slug>.md`.

- If this session already runs Fable, plan **inline**: read the code yourself and write the file. Cheaper than a
  cold subagent, and it keeps your context.
- Otherwise start one agent: `subagent_type: "Plan"`, `model: "fable"`. Give it the task, the plan path, any
  constraints from the conversation, and the template below. The Plan agent is read-only, so have it **return the
  plan text** and you write the file.

Plan template (the planner must fill every section):

```markdown
# <Title>

Goal: <one or two sentences — what is true when this is done>
Out of scope: <what not to touch>

## Context
<the facts an implementer needs that are not obvious from the files: conventions, gotchas, related systems,
UE specifics (Blueprint vs C++, which level/asset, editor-only vs runtime)>

## Steps
### S1 — <imperative title>   [routine|tricky]   deps: none
Files: <exact paths; new files marked (new)>
Do: <concrete instructions: functions/classes/properties to add or change, names, signatures>
Accept: <how to verify this step: compiles, a test, a log line, a PIE behavior, a screenshot>

### S2 — ...   [routine]   deps: S1

## Final check
<the end-to-end verification: build command, PIE scenario, what to look for>

## Risks / open questions
<anything the planner could not decide; each with a default the implementer should use>
```

Planner rules to pass along:
- Name exact files, symbols and signatures. Vague plans are where cheaper implementers fail.
- `tricky` = new system, math/physics, concurrency, debugging, or a step where real design choices remain.
  Everything else is `routine`.
- Steps that touch the same file must be chained with `deps`, so they never run in parallel.
- Keep steps small enough that one agent finishes one in a single pass (roughly ≤ 300 changed lines).

## 2. Approve

Unless `--no-approve`: show the user the plan path and a short summary (steps, tags, parallel groups), then ask
for go / changes. Edit the plan on feedback. Do not start implementing until they say go.

## 3. Implement (Sonnet / Opus)

Group steps into waves by `deps`. Within a wave, steps with disjoint `Files:` run in parallel (several Agent calls
in one message); otherwise one at a time.

For each step start an agent: `subagent_type: "general-purpose"`, `model:` from `--impl`, or with `auto`:
`routine` → `"sonnet"`, `tricky` → `"opus"`. Prompt:

> Implement step <Sn> of the plan at `<plan path>`. Read the whole plan first for context, then do only <Sn>.
> Follow it exactly. If the code doesn't match what the plan assumes, or the plan is ambiguous in a way that
> changes behavior, STOP and report — do not improvise a redesign. Run the step's Accept check.
> Do not commit. Report: status (done / blocked), files changed, Accept result, and any deviation from the plan.

After each wave:
- `blocked` → read the report. Small gap: fix the plan yourself and rerun the step. Real design problem: send it
  to an Opus agent with the report, or ask the user.
- A failed Accept on a `routine` step gets one retry on Opus before you escalate to the user.
- Tick the step in the plan file (`### S1 — ... ✅`) so a resumed run knows where it stopped.

**Unreal specifics.** Implementers inherit the session's tools, including `unreal-mcp`. If a step's Accept needs
the editor (asset edits, PIE, Blueprint), make sure it is running first (ue-editor skill) and run those steps one
at a time: only one agent should drive the editor. C++ changes need a build before PIE.

## 4. Review (Fable)

Unless `--no-review`: start an agent with `model: "fable"` (or review inline if you are Fable). Give it the plan
path and `git diff` (plus untracked new files). Ask it to check:
- every step done as specified, nothing out of scope touched;
- correctness bugs, missed edge cases, conventions of the surrounding code;
- the plan's Final check — run it if it can, or say exactly what is unverified.

Send each real finding to an Opus implementer to fix, then re-run only the affected checks. Do not loop the review
more than twice; leftover findings go to the user.

## 5. Report

Tell the user: plan path, steps done (and on which model), checks that passed, anything unverified or deviated,
review findings left open. Nothing is committed — offer to commit.

## Notes

- Cost: the planner and reviewer read a lot but write little; implementers write most of the tokens. That is
  where the cheaper models pay off.
- Hosts without a per-agent `model` parameter (e.g. Codex): run the same steps, but plan and review in the main
  session and tell the user which model each phase actually ran on.
