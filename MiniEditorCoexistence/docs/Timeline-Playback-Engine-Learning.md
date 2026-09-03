# Timeline Playback Engine — A Living Architecture and Learning Journal

*Slide 1 — title*

MFC + Qt coexistence study

ADR-001 through ADR-007 accepted — implementation next

*Converted from the companion slide deck,
[`Timeline-Playback-Engine-Learning.pptx`](Timeline-Playback-Engine-Learning.pptx).
Each section below is numbered to match its source slide. Source citations
are carried over from each slide's speaker notes.*

---

## Slide 2 — The current editor has two playback clocks

**Source preview**

- `QMediaPlayer` is authoritative
- Its position callbacks update `EditorSession`
- Decoded video arrives through `QVideoSink`

**Timeline preview**

- MFC timer + `EditorSession` are authoritative
- The timer advances one timeline frame
- Players follow resolved V1 and A1 sources

The meaning of "current time" changes with preview context.

*Sources:*
- `docs/architecture/current-playback-architecture.md`

---

## Slide 3 — Timeline playback coordinates three authorities

```text
MFC timer (33 ms cadence)
    -- clock -->
EditorSession (timeline head)
    -- state -->
Timeline resolver
    -- media clocks -->
V1 player + A1 player
    -->
Preview + audio output
```

They begin from related positions, but no master clock measures drift.

*Sources:*
- `src/MainFrame.cpp`
- `src/QtMediaPlaybackBackend.cpp`
- `src/PlaybackClockController.cpp`

---

## Slide 4 — One playback session is the authority

```text
Immutable snapshot
    -->
Timeline resolver
    -->
Async video + audio decode
    -->
Scheduler + master clock
    -->
Compositor + frame queue
    -->
Preview + audio output
```

UI focus may request playback; it never defines transport identity.

*Sources:*
- `docs/architecture/current-playback-architecture.md`
- Target architecture is a project design proposal, not an external claim.

---

## Slide 5 — Every design decision must answer four questions

**Ownership**
Who is allowed to mutate this state?

**Authority**
Whose answer wins when clocks disagree?

**Thread affinity**
Where may this object be called and destroyed?

**Stale work**
How is an obsolete async result rejected?

*Sources:*
- `docs/architecture/current-playback-architecture.md`

---

## Slide 6 — Seven accepted contracts shape the engine

| Contract | Status | Highlights |
| --- | --- | --- |
| Time contracts | **Accepted** | ADR-001 — 8 strong types, explicit conversions |
| Authority + state | **Accepted** | ADR-002 + ADR-003 — single transport authority, immutable async state |
| Clock + threads | **Accepted** | ADR-004 + ADR-005 — master-clock policy, explicit thread boundaries |
| Identity + UI | **Accepted** | ADR-006 + ADR-007 — explicit sequence identity, framework-neutral adapters |

*Sources:*
- `docs/architecture/current-playback-architecture.md`
- Roadmap is the agreed project implementation plan.

---

## Slide 7 — Three identities reject three kinds of stale work

**Snapshot revision**
Which committed editor state does playback read?

**Playback generation**
Which transport epoch may affect playback?

**Presentation request**
Which valid frame does this viewport want now?

**Acceptance rule**
All relevant identities must still match.

*Sources:*
- `docs/architecture/current-playback-architecture.md`
