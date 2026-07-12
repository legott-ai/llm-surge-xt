# DeepSynth for Surge XT

DeepSynth For Surge XT is a GPL-3.0-or-later fork of [Surge XT](https://surge-synthesizer.github.io/)
that adds an in-app, natural-language patch designer. Describe the sound you
want in English or Korean and DeepSynth translates it into Surge XT parameter
changes for the currently loaded patch.

This project is a derivative work of Surge XT. It is not affiliated with or
endorsed by the Surge Synth Team, Anthropic, Google, or OpenAI. Original Surge
XT source files retain their upstream copyright headers.

## Features

- Generate a new patch or modify the current patch from a text description.
- Use Anthropic Claude, Google Gemini, or OpenAI models from the DeepSynth panel.
- Map friendly parameter names to live Surge XT parameters.
- Optionally retrieve similar factory patches to add context to generation.
- Keep provider keys in local application settings; keys are never committed.

## Architecture

```
prompt -> DeepSynth panel -> provider API -> parameter changes -> Surge XT patch
                              ^
                              | optional similar-patch context
```

The implementation is concentrated in these areas:

| Path | Purpose |
| --- | --- |
| `src/common/AIModelProvider.*` | Provider abstraction and model selection |
| `src/common/ClaudeAPIClient.*` | Anthropic API client |
| `src/common/GeminiAPIClient.*` | Gemini API client |
| `src/common/OpenAIAPIClient.*` | OpenAI API client |
| `src/common/ClaudeParameterMapper.*` | AI parameter-name mapping and application |
| `src/common/PatchVectorDB.*` | Similar-patch retrieval |
| `src/common/PatchParameterExtractor.*` | Factory-patch feature extraction |
| `src/surge-xt/gui/panels/DeepSynthPanel.*` | In-app DeepSynth interface |

## Build

Clone the repository, configure CMake, then build the desired target. The
required third-party source is included in this repository.

```bash
git clone https://github.com/legott-ai/llm-surge-xt.git
cd llm-surge-xt
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target surge-xt_Standalone --parallel 8
```

On macOS, launch the resulting standalone application with:

```bash
open "build/surge_xt_products/Surge XT.app"
```

DeepSynth requires a C++17 compiler and CMake 3.15 or later. If a current
Xcode/clang toolchain stops in bundled DSP code because of `-Werror`, configure
with Surge XT's supported escape hatch:

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DSURGE_SKIP_WERROR=TRUE
```

For upstream platform setup, packaging, plug-in formats, and contributor
workflow, see the [Developer Guide](doc/Developer%20Guide.md) and
[Git guide](doc/How%20to%20Git.md).

## Use DeepSynth

1. In Surge XT, choose **DeepSynth -> Show Panel**.
2. Open **Settings** and add an API key for Claude, Gemini, or OpenAI.
3. Select a model, describe the sound, then choose **Generate** or **Modify**.

Provider keys are sent only to their respective API in request headers. Set
`DEEPSYNTH_DEBUG=1` before launching Surge XT to enable diagnostic logging for a
session; logging is disabled by default.

## License

DeepSynth and Surge XT are distributed under [GPL-3.0-or-later](LICENSE).
