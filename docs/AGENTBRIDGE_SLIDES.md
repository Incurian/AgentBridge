# AgentBridge Presentation Slides

*4 slides for a presentation on AgentBridge tooling strategies.*
*Based on `docs/AGENTBRIDGE_ANALYSIS.md` and `docs/agent_autonomy.md`.*

---

## Slide 1: "What We Built vs. What We Didn't"

**Layout:** Two-column comparison with a unifying thesis at the bottom.

### Left Column: "Employed"

| Strategy | One-liner |
|----------|-----------|
| Layered absorption | 5 layers, each hides a category of UE complexity |
| Modular tool loading | 6 profiles from 6 to ~100 tools; load more at runtime |
| Smart defaults & auto-detection | Class names, asset paths, value types auto-corrected |
| Multi-strategy resolution | 4 tries for classes, 6 tries for actors, 3 for components |
| Self-documenting help system | `help()` tool teaches agents UE workflows on demand |
| Structured error responses | Errors include hints, suggestions, and similar-name lookups |
| Unified syntax over multiple backends | One `call_function` tool routes to 3 different backends |
| Offline asset path | 14 tools work without the editor via JSON round-trip |

### Right Column: "Deliberately Avoided"

| Strategy | Why not |
|----------|---------|
| Planning / agent framework | The LLM already plans; don't build a worse planner |
| Natural language interface | Adds an interpretation layer that fails silently |
| Visual feedback loop | Structured data is faster and more verifiable |
| Sandbox / preview mode | UE state is too interconnected for meaningful preview |
| Tool composition / macros | The LLM is the composition layer |
| Domain-specific language | Generic property paths work for any class, present or future |
| Confirmation prompts | The host application handles permissions |
| Fine-tuned / specialized model | Couples to UE version; help system is "runtime fine-tuning" |

### Bottom Banner (thesis)

> **"Dumb at the top, smart at the bottom."**
> Intelligence budget spent on *forgiveness* (making wrong inputs work),
> not *autonomy* (deciding what to do next).

### Speaker Notes

The core design philosophy is: don't duplicate what the LLM already provides. The LLM
is already a planner, an interpreter, a composition engine. AgentBridge focuses on being
the part the LLM can't be - a reliable, forgiving bridge to Unreal Engine's internals.

The left column is where the engineering effort went: making imprecise inputs work
through resolution cascades and auto-detection. An agent can write `spawn_actor("BP_Tree")`
and the system tries BP_Tree, BP_Tree_C, case-insensitive search, and path loading
before giving up. That forgiveness is more valuable than any amount of planning logic,
because the LLM is already smart - it just doesn't know UE naming conventions.

The right column represents deliberate restraint. Each of these is a common pattern in
agent tooling that AgentBridge chose not to implement. The recurring reason: it would
create a second, worse version of something the LLM already does. A natural language
interface adds an interpretation layer on top of an LLM that already interprets natural
language. A planning layer adds a planner on top of a model that already plans. In each
case the added complexity creates new failure modes without proportional benefit.

---

## Slide 2: "Tradeoffs"

**Layout:** Two side-by-side boxes (Pros left, Cons right), with a key tension callout
at the bottom.

### Left Box: "What This Gets You" (green)

- **Low cognitive load for agents** - ~90% of "wrong" inputs auto-correct through
  resolution cascades; agents rarely need retries for naming/formatting

- **Right-sized toolkits** - Profiles prevent tool overload; a terrain agent doesn't
  see sensor tools

- **Actionable errors** - "Use instance name 'LightComponent0' instead of class name
  'PointLightComponent'" beats "property not found"

- **Three deployment paths** - gRPC (fast, live), HTTP (simple, live), offline JSON
  (no editor needed)

- **Zero domain training required** - Help system teaches UE concepts at query time;
  works with any general-purpose LLM

### Right Box: "What It Costs You" (red)

- **No autonomy** - Can't point it at a goal and walk away; needs an orchestrator

- **No error recovery** - Tells you what went wrong, but not how to fix it; agent must
  figure out recovery

- **No undo/rollback** - Spawn 50 wrong actors? Delete them one by one

- **No reactive workflows** - Request-response only; can't subscribe to events or
  get notifications

- **Still ~35 tools minimum** - Even the default profile is a lot for small context windows

- **Monolith risk** - CommandExecutor.cpp is 7,400 lines; agentbridge.py is 3,000 lines

### Bottom Callout (key tension)

> **The fundamental tension: generality vs. guardrails.**
> General tools + powerful model = flexible and capable.
> General tools + weak model = confused and error-prone.
> The system is optimized for the first case.

### Speaker Notes

The pro/con split maps directly to the "forgiveness over autonomy" philosophy. The pros
are all consequences of investing in forgiveness: inputs auto-correct, errors are
descriptive, deployment is flexible. The cons are all consequences of not investing in
autonomy: no self-correction, no undo, no event-driven behavior.

The key tension at the bottom is the crux of the design. AgentBridge works extremely
well with frontier models (Claude, GPT-4 class) because those models can select from
35 tools, compose multi-step plans, and recover from errors on their own. But the same
design becomes a liability with weaker models that struggle with tool selection and
can't reason through error recovery. This leads directly to slide 3.

The monolith concern (CommandExecutor.cpp at 7,400 lines) is worth mentioning because
it's the most likely scaling bottleneck. Every new operation touches that file. It works
today but will create merge conflicts and cognitive load as the feature set grows.

---

## Slide 3: "Adapting for Less Capable Models"

**Layout:** A vertical spectrum/gradient from top (powerful model, current design) to
bottom (weak model, maximum guardrails), with adaptation strategies placed along it.

### The Spectrum

```
MORE CAPABLE MODEL
  |
  |  Current AgentBridge design
  |    - ~35 general-purpose tools
  |    - Agent decides strategy
  |    - Errors returned as data
  |
  |  ---- Reduce tool count ----
  |    Minimal profile: 8-10 tools covering core operations
  |
  |  ---- Add composite tools ----
  |    spawn_light(type, location, intensity, color)
  |    instead of spawn + set_property + set_property
  |
  |  ---- Add guided workflows ----
  |    start_workflow("build_scene") returns next step
  |    Model fills in params, system decides sequence
  |
  |  ---- Return structured recovery ----
  |    Error includes: {recovery_actions: [{tool, params, reason}]}
  |    Model picks from offered fixes, doesn't invent them
  |
  |  ---- Add session state tracking ----
  |    get_session_state() shows what agent has created/modified
  |    Model doesn't need to maintain mental state
  |
  |  ---- Typed action envelopes ----
  |    Tools declare preconditions, effects, safety invariants
  |    System blocks calls that violate invariants
  |
  |  ---- Domain-specific validated tools ----
  |    set_light_intensity(actor, value) with range validation
  |    instead of generic set_property
  |
  |  Maximum guardrails design
  |    - Many specialized tools, each trivial to use
  |    - System decides strategy
  |    - Errors come with pre-built fixes
  |
LESS CAPABLE MODEL
```

### Bottom Takeaway

> Each step down the spectrum trades **generality for safety**.
> The right position depends on the model, not the system.
> AgentBridge's modular loading makes it possible to offer
> different positions to different callers.

### Speaker Notes

This is the forward-looking slide. The spectrum represents a concrete path for adapting
AgentBridge to less capable models without rewriting the core.

The top of the spectrum is where AgentBridge sits today: general tools that assume a
capable orchestrator. As you move down, each adaptation constrains the agent more but
makes success more likely for weaker models.

The key adaptations, in order of impact:

1. Reduce tool count. Going from 35 to 10 tools dramatically improves tool selection
   accuracy for smaller models. This is free - just define a new profile.

2. Composite tools. Instead of requiring the agent to know that setting a light's color
   requires spawning, then finding the component name, then calling set_property with a
   specific path - just offer `spawn_light(color="warm_white")`. Fewer decisions, more
   encapsulated.

3. Guided workflows. This is the biggest conceptual shift. Instead of the agent deciding
   the sequence of operations, the system returns "here's your next step." The agent
   becomes a parameter-filler rather than a planner. Similar to a setup wizard vs. a
   command line.

4. Structured recovery. Instead of error messages that the agent must interpret and act
   on, return a menu of recovery options: `[{tool: "list_classes", params: {...},
   reason: "Find similar classes"}]`. The agent picks from options rather than inventing
   a fix.

5. Session state. Weak models lose track of what they've done across many tool calls.
   Exposing a `get_session_state()` tool that returns "you've spawned 3 actors, modified
   2 properties, and have 1 error" acts as external memory.

6. Typed action envelopes. This comes from the broader agent autonomy literature
   (see `docs/agent_autonomy.md`). Today, AgentBridge's MCP tools declare parameter
   schemas via JSON Schema - what *types* are valid. Action envelopes extend this with
   *preconditions* ("actor must exist"), *effects* ("actor will be deleted"), and *safety
   invariants* ("never delete more than 10 actors in one session"). The system can then
   block calls that would violate invariants *before execution*, rather than reporting
   errors after the fact. MCP's existing tool schema could be extended with these fields
   without changing the protocol. For weaker models this is powerful: the system prevents
   dangerous actions rather than trusting the model to avoid them.

   A trust accumulator could gate which envelope level applies: a new agent starts
   with tight invariants (Level 1 - only select from pre-approved actions). As it
   demonstrates reliability across many calls, the envelope expands (Level 3 - propose
   novel tool calls, validated by a lightweight verifier). A single bad action tightens
   the envelope back. Trust accrues slowly, erodes quickly - mirroring prospect theory's
   asymmetry between gains and losses.

7. Domain-specific validation. The nuclear option: replace general tools with many
   specific ones that each validate their domain. `set_light_intensity` knows that
   intensity must be positive and probably shouldn't exceed 100,000. Generic
   `set_property` doesn't.

The bottom takeaway is important: the modular loading system already provides the
mechanism for this. Different profiles can expose different tool sets to different
callers. A frontier model gets the `full` profile. A 7B model gets a hypothetical
`guided` profile with composite tools and workflow sequencing. Same backend, different
surface.

---

## Slide 4: "Where This Fits in the Autonomy Landscape"

**Layout:** A horizontal band diagram showing the 12-band autonomy spectrum (simplified
to 5 zones). AgentBridge is highlighted at its position. Arrows show where it could
extend. A bottom section shows the "compositional stack" vision.

### The Autonomy Spectrum (simplified)

```
HIGH AUTONOMY                                              LOW AUTONOMY
|                                                                    |
| Autonomous    Multi-Agent   Graph-Based    Tool-Use    Behavior    |
| Agents        Orchestration Workflows      Protocols   Trees &     |
|                                                        Deterministic|
| (AutoGPT,     (CrewAI,      (LangGraph,   (MCP,       (BT.CPP,    |
|  Claude Code,  MetaGPT,      Haystack)     AgentBridge) GOAP,      |
|  Manus)        AutoGen)                     <-- HERE    PLC)       |
|                                                                    |
```

### AgentBridge's Position

- **Band 5: Tool-use protocol.** Typed action space, structured schemas, host controls
  execution and consent. Agent can only invoke functions matching declared schemas.
- **Not Band 1** (no autonomous goal decomposition, no self-correction loop)
- **Not Band 3** (no graph-based workflow structure, no explicit state machine)
- **Not Band 10** (no behavior tree skeleton, no deterministic fallback)

### Where It Could Extend (arrows on diagram)

| Direction | What it adds | Architecture |
|-----------|-------------|--------------|
| Toward Band 3 (left) | Guided workflows, stateful sessions | Workflow engine on top of existing tools |
| Toward Band 10 (right) | Behavior-tree skeleton with tool leaf nodes | BT provides structure; LLM fills in decisions at leaves |
| Toward Band 7 (overlay) | Safety guardrails, action-sequence verification | Middleware between agent and tool execution |

### The Compositional Stack (bottom section)

```
+-----------------------------------------------------------+
| Layer 5: Behavioral structure (BT / workflow / planner)    | <-- NOT YET
+-----------------------------------------------------------+
| Layer 4: Safety guardrails (action-sequence verification)  | <-- NOT YET
+-----------------------------------------------------------+
| Layer 3: Typed tool schemas (MCP / JSON Schema)            | <-- AgentBridge TODAY
+-----------------------------------------------------------+
| Layer 2: Structured output (constrained decoding)          | <-- Model provider
+-----------------------------------------------------------+
| Layer 1: Token-level generation (LLM)                      | <-- Model provider
+-----------------------------------------------------------+

Each layer adds a different kind of constraint.
The combination provides defense in depth.
AgentBridge owns Layer 3. The opportunity is Layers 4 and 5.
```

### Bottom Takeaway

> **AgentBridge provides one layer of a multi-layer safety stack.**
> The industry is converging on a universal pattern:
> *propose within bounds, verify, escalate when uncertain.*
> AgentBridge implements "propose within bounds" (typed schemas).
> The next steps are "verify" (action-sequence guardrails) and
> "escalate" (graduated autonomy with trust accumulation).

### Speaker Notes

This slide places AgentBridge in the broader context of AI agent autonomy research. The
12-band spectrum comes from a separate analysis (`docs/agent_autonomy.md`) covering
everything from fully autonomous agents (Band 1: Manus, Claude Code) down to
deterministic industrial control (Band 12: PLCs running on 1ms scan cycles).

AgentBridge sits squarely at Band 5: tool-use protocols. The MCP standard (now under the
Linux Foundation with 97M+ monthly SDK downloads) defines exactly this pattern - typed
schemas, host-controlled execution, structured request-response. AgentBridge is a
concrete implementation of Band 5 for the Unreal Engine domain.

The interesting insight from the autonomy research is that production systems don't pick
one band - they compose multiple bands into a stack. The industry is converging on a
universal pattern that appears independently in aerospace (Simplex architecture), robotics
(RSS/safety envelopes), and AI agents (propose-verify-escalate):

1. An advanced controller proposes actions (the LLM)
2. A monitor verifies they're safe (guardrails, typed schemas)
3. A fallback handles violations (safer controller, human escalation)

AgentBridge currently implements step 2 at the schema level: tools declare what
parameters are valid, and invalid calls are rejected. But it doesn't verify *sequences*
of actions (you can delete every actor in the level, one valid call at a time) and it
doesn't have a fallback mode (if the agent makes a mistake, there's no safer controller
to switch to).

The extension arrows on the diagram show three concrete paths:

**Toward workflows (Band 3):** Add a lightweight workflow engine that guides agents
through multi-step procedures. This is Slide 3's "guided workflows" adaptation, but
framed architecturally - it's adding a Band 3 layer on top of the existing Band 5 tools.
The behavior is: workflow defines the sequence, LLM fills in parameters at each step.

**Toward behavior trees (Band 10):** Use a behavior tree as the structural skeleton with
AgentBridge tools as leaf nodes. The BT provides deterministic structure (sequences,
fallbacks, condition checks), guaranteed reactivity (re-evaluates every tick), and bounded
execution time (each LLM call has a timeout with a fallback branch). The LLM decides
*how* to execute each leaf, but the tree decides *what* to execute and *in what order*.
This is the strongest architecture for weak models - they only need to fill in leaf-node
parameters, not plan the overall sequence.

**Toward guardrails (Band 7):** Add a middleware layer between the agent's tool selection
and tool execution that verifies action sequences against safety properties. For example:
"never delete more than N actors without confirmation," "always save level before
spawning more than 20 actors," "don't modify landscape and lighting in the same session."
These can be expressed as temporal logic specifications and monitored in real-time with
minimal overhead. The key research result (from compositional shield synthesis) is that
adding a new tool only requires defining its *category* and adding category-level safety
properties - you don't need to re-verify the entire action space.

The compositional stack at the bottom is the key forward-looking idea. AgentBridge owns
Layer 3 today. The model provider owns Layers 1-2 (and these are rapidly improving -
constrained decoding now ships in all major APIs with near-zero overhead). The opportunity
for AgentBridge is Layers 4 and 5: safety guardrails and behavioral structure. These
don't require replacing the current architecture - they layer on top of it, adding
constraint without removing flexibility.

---

## General Presentation Notes

**Recommended format:** 16:9 widescreen, dark background, high-contrast text.

**Visual style for the spectrum on Slide 3:** Use a vertical gradient from green (top,
current) to blue (bottom, future). Place each adaptation as a horizontal divider across
the gradient. This makes the "tradeoff spectrum" visually intuitive.

**Visual style for Slide 4:** The autonomy band diagram should be horizontal (left =
high autonomy, right = low autonomy) with AgentBridge highlighted in a distinct color.
The extension arrows can be colored to match their target band. The compositional stack
at the bottom should be a literal stack diagram with Layer 3 highlighted and Layers 4-5
shown with dashed borders (not yet implemented).

**Slide 1** is information-dense by design - it's the reference slide people will
photograph. Consider animating the two columns to appear sequentially (employed first,
then avoided) to give the audience time to absorb.

**Slide 2** is the "so what" - keep it clean. The two boxes should be visually balanced.
The bottom callout is the thesis of the slide; make it larger than the box content.

**Slide 3** is the model-capability payoff. The spectrum is the actionable contribution -
it shows a concrete path from "works with Claude" to "works with Llama-7B" without
starting over.

**Slide 4** is the research-context payoff. It connects AgentBridge to the broader
autonomy landscape and shows where the field is heading. If the audience is more
practitioner than researcher, this slide can be presented quickly as "where we fit and
where we're going." If the audience is more academic, spend time on the compositional
stack and the propose-verify-escalate pattern.

**Narrative arc across all 4 slides:**
1. *What we did and didn't do* (design choices)
2. *What that costs and buys* (tradeoffs)
3. *How to serve weaker models* (practical adaptations)
4. *Where this fits in the big picture* (research context and future)

**If you need to cut to 3 slides:** Merge slides 3 and 4 by keeping the Slide 3 spectrum
but replacing the bottom takeaway with the Slide 4 compositional stack diagram. Drop
the autonomy band diagram and the extension arrows - they're informative but not
essential.
