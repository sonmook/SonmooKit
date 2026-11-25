# Compilar e testar no Windows

[← Voltar ao README](../README.md)

Requer Visual Studio 2022 com **Desktop development with C++**, Windows SDK, Git, CMake 3.22 ou superior e conexão para obter o JUCE. Execute na raiz do repositório, onde fica o `CMakeLists.txt`.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target StereoEcho_VST3 SoftCompressor_VST3 PeakCeiling_VST3 SibilanceControl_VST3 DriveTone_VST3 CurveTen_VST3 VoiceStack_VST3 PulseEQ_VST3 FETStrike_VST3 PitchPilot_VST3 OptoLevel2A_VST3 StereoArc_VST3 AirLift_VST3 BassBody_VST3 SweepTone_VST3 DrumPress_VST3 LevelLift_VST3 BeatDip_VST3 RoomBloom_VST3
```

Os bundles completos ficam em `build/<Efeito>_artefacts/Release/VST3/`. Copie cada pasta `.vst3` inteira para `C:\Program Files\Common Files\VST3\SonmooKit`. No FL Studio, abra **Options > Manage plugins**, ative **Verify plugins** e faça a busca. Carregue os efeitos em slots do Mixer. Consulte o [manual de instalação de plugins do FL Studio](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).

## Validação no host

- StereoEcho. Use uma batida curta e renderize com o mix a 100%. Teste 375 ms, sincronização em 1/4 a 120 BPM e a alternância estéreo em ping-pong.
- SoftCompressor. Use um tom de nível constante. Com knee em 0, threshold em −18 dB, ratio 4 e mix a 100%, um pico de −6 dBFS deve estabilizar próximo de −15 dBFS.
- PeakCeiling. Use um sinal com transientes acima do threshold e verifique o teto de pico de amostra. Confira alinhamento com a compensação de latência do Mixer. O teto se aplica a amostras digitais.
- Salve e reabra o projeto do FL Studio para verificar a restauração de parâmetros.

O alvo `PitchPilot_VST3` gera o plugin exibido no Mixer como **SonmooTune**.

## Verificador da série de um controle

Para executar os ensaios no VST3 compilado, acrescente a opção abaixo e indique o caminho da pasta do plugin ao verificador. O programa encerra com código zero quando o efeito passa em todas as taxas de amostragem. Repita o último comando para os oito nomes da série.

```powershell
cmake -S . -B build -DSONMOOKIT_BUILD_MACRO_CHECK=ON
cmake --build build --config Release --target MacroCheck AirLift_VST3
.\build\Release\MacroCheck.exe ".\build\AirLift_artefacts\Release\VST3\AirLift.vst3"
```
