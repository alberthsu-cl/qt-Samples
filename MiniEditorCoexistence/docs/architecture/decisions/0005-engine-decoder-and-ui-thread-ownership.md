# ADR-005: Engine, Decoder, Audio Callback, and UI Thread Ownership

Status: Proposed

Date: 2026-09-02

## Context

ADR-002 assigns transport authority to `PlaybackSession`. ADR-003 defines
immutable snapshots and generation-gated presentation. ADR-004 defines clock
selection and scheduler timing. This ADR defines where the remaining runtime
objects execute and how they communicate.

The migration must allow Qt and MFC UI layers to coexist while decode,
composition, audio delivery, and playback scheduling run asynchronously. UI
objects must remain on the GUI thread; decoder callbacks must not mutate
editor state; audio callbacks must meet real-time constraints; and shutdown
must not destroy an object while queued work can still call it.

## Decision

The runtime has four ownership domains:

| Domain | Owns | May communicate through |
| --- | --- | --- |
| GUI/UI thread | `EditorSession`, Qt widgets, MFC windows, view state, editor commands | queued UI commands and immutable publications |
| Engine thread | `PlaybackSession`, scheduler, anchor, generation, snapshot install | serialized engine command queue |
| Decoder/compositor workers | decoder instances and frame work for one request | bounded work queues and immutable results |
| Audio callback/device thread | audio ring-buffer consumption and device observations | non-blocking audio port only |

No object is owned by two domains. A thread may read an immutable value
published by another domain, but it never reads that domain's mutable state.

## GUI/UI thread

The GUI thread owns every Qt widget, MFC window handle, model/view selection,
and editor command boundary. It may build and publish a
`SequencePlaybackSnapshot` after an edit transaction, submit commands to the
engine, and consume immutable `PlaybackStatus` or frame publications.

The GUI thread must not:

- call decoder APIs directly;
- block waiting for a frame, audio buffer, or engine command;
- mutate `PlaybackSession` state directly;
- pass a widget, `QObject*`, `HWND`, or `CDC` across a worker boundary.

Qt and MFC adapters translate user actions into framework-neutral commands and
marshal results back to the GUI thread. The adapters are replaceable; the
engine does not know which UI framework issued a command.

## Engine thread and command queue

One engine thread owns the mutable runtime state of one `PlaybackSession`:
phase, current snapshot, sequence position, generation, clock selection,
anchor, and pending transport intent. All commands are serialized through an
engine command queue. The engine thread is the only writer of those values.

An engine command is a value object. It contains no Qt/MFC object or mutable
editor reference. The command queue is bounded or back-pressured by policy.
Commands include play,
pause, stop, seek, rate change, source replacement, snapshot installation, and
shutdown; clock selection is an engine-internal decision governed by ADR-004,
not a new caller-facing command. A command that is superseded by a newer seek
or snapshot may have its scheduled decode/composition work coalesced, but the
command itself is still individually accepted or rejected and acknowledged per
ADR-002. Command ordering must remain observable at the engine boundary. The
queue never executes user callbacks inline on a producer thread.

## Decoder and compositor workers

Decoder and compositor workers own only their decoder/service objects and
temporary resources. A worker receives an immutable request containing the
`PlaybackWorkIdentity`/`SequenceWorkIdentity` from ADR-003 and a snapshot
pointer. It may finish after cancellation; the consumer rejects the result if
session, generation, revision, media, or request identity is stale.

Video work uses ADR-003's bounded latest-wins policy. Audio work uses the
buffering policy from ADR-004. Workers never access widgets, editor models,
`PlaybackSession` mutators, or the engine anchor.

Worker completion is delivered as an immutable result to the engine or
presentation coordinator through a queue. The completion callback itself does
not decide whether a result is current; identity validation at the consumer is
the authority. A decode failure or an unavailable-media descriptor is delivered
as an immutable failure result; the engine consumer validates its identity and
performs the `PlaybackPhase::Failed` transition with a framework-neutral
`PlaybackError`. The detailed decoder error taxonomy remains an implementation
contract owned by this ADR and may be refined without exposing Qt types.

This worker domain may later split into dedicated decoder and GPU-compositor
domains when D3D11/QRhi composition is introduced; the ownership and result
identity rules remain unchanged.

## Audio callback and device boundary

The audio callback/device thread is a real-time boundary. It consumes already
prepared audio samples from a ring buffer and publishes minimal observations
such as device position, consumed sample count, or underflow status.

The callback must not block, allocate, perform file or decoder I/O, call into
Qt widgets, acquire a mutex contended by the engine thread, or mutate playback
authority. It never waits for video. Device observations cross into the engine
through a lock-free or otherwise non-blocking handoff defined by the audio
adapter.

The engine decides clock selection, generation changes, underflow recovery, and
transport phase. The callback only reports facts about the device boundary.

## Qt object affinity and queued delivery

Every `QObject` has one owning thread and is used only from that thread unless
the Qt API explicitly documents a thread-safe operation. Signals carrying
cross-thread data use queued delivery and registered value types. Published
frames, statuses, errors, and commands are immutable values or shared immutable
resources.

`deleteLater()` is posted to the object's owning event loop. It is valid only
while that loop can process the deferred deletion. Shutdown therefore follows
this order:

1. GUI thread submits an engine shutdown command.
2. Engine stops scheduling work, invalidates its generation, and asks workers
   and audio adapters to stop accepting new work.
3. Engine waits for application-owned workers and decoder resources to finish.
4. Engine publishes the final ADR-002 shutdown acknowledgment status, with the
   playback phase unchanged, before command acceptance closes; it closes
   command acceptance immediately afterward.
5. The owning event loops process queued completions and deferred deletions.
6. The engine thread exits; the GUI thread destroys UI adapters and widgets.

No worker completion is allowed to call a deleted receiver. Queued signals are
either disconnected during the ownership transition or carry a lifetime-safe
handle whose consumer still performs identity validation.

## MFC coexistence boundary

MFC windows remain GUI-thread objects. An MFC adapter posts framework-neutral
commands to the engine and receives immutable publications through the MFC
message loop using the target architecture's UI notification bridge. A Qt
adapter follows the same contract using queued signals or event-loop posts.
The legacy pattern of calling `QCoreApplication::processEvents()` from an MFC
timer is retired by this model. Neither adapter becomes a second playback
authority.

This permits gradual migration: a Qt preview panel can consume the same engine
publication as an MFC preview panel, and either adapter can be removed without
changing engine, decoder, or clock ownership.

## Acceptance criteria

ADR-005 is implemented when:

1. Thread-affinity tests prove UI objects are created, used, and destroyed on
   the GUI thread.
2. Engine-state tests prove only the engine thread mutates phase, snapshot,
   anchor, generation, and transport position.
3. Commands and asynchronous results contain no Qt/MFC objects or mutable
   editor references.
4. Consumers reject stale decoder/compositor results using ADR-003 identities;
   workers never decide their own currency.
5. A validated decode failure or unavailable-media result transitions the
   session to `PlaybackPhase::Failed` with a `PlaybackError`; a stale failure
   result does not.
6. Audio-callback tests prove no blocking, allocation, UI call, or engine-lock
   contention occurs on the callback path.
7. Audio underflow and device observations reach the engine without allowing
   the callback to mutate playback authority.
8. Shutdown tests prove queued work cannot call destroyed receivers and every
   application-owned worker is joined before its owner is destroyed.
9. Qt `deleteLater()` objects are deleted only by their owning event loop.
10. MFC and Qt adapters can issue the same engine commands and consume the same
   immutable publications without duplicating transport state.
11. Core ownership tests run without Qt Widgets, MFC windows, real codecs, or
   hardware.

## Consequences

Thread ownership is explicit and testable. UI migration can proceed one panel
at a time because framework adapters share a stable engine boundary. The cost
is additional value types, queues, and shutdown coordination; those costs are
intentional safeguards against races and use-after-destroyed-object failures.

## Deferred decisions

This ADR does not choose a particular thread-pool implementation, decoder
backend, lock-free queue library, audio API, or GPU compositor. Those are
implementation choices as long as the ownership and real-time constraints
remain true.
