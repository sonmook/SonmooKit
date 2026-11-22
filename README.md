# SonmooKit para FL Studio

Projeto C++ com JUCE 8.0.6. Gera sete efeitos VST3 x64 independentes, com interface própria, medidores em tempo real, parâmetros automatizáveis e restauração de estado no projeto.

| Efeito | Referência funcional | Controles |
| --- | --- | --- |
| StereoEcho | H-Delay | Tempo em milissegundos ou sincronizado ao BPM do host, feedback, filtros, ping-pong e mix |
| SoftCompressor | Renaissance Compressor | Threshold, ratio, ataque, release, knee, makeup e mix paralelo. O detector de pico aplica o mesmo ganho aos dois canais |
| SibilanceControl | Sibilance | Limiar, faixa de redução, largura da detecção, transição Wide/Split, audição do sinal removido, lookahead de 12 ms e histórico do detector |
| DriveTone | OneKnob Driver | Drive de 0 a 10, tonalidade, ajuste de saída e mix |
| PeakCeiling | L1 | Threshold, teto de pico de amostra em dBFS, release e lookahead fixo de 2 ms com latência reportada ao host |

| CurveTen | Q10 | Dez bandas, seis tipos de filtro, curva arrastável, espectro de saída e ganho global |
| VoiceStack | Doubler | Quatro vozes com ganho, pan, delay, detune, feedback, oitava e modulação por voz |

## Versão 0.5.0

CurveTen e VoiceStack são efeitos com algoritmos próprios. CurveTen aplica as mesmas bandas aos dois canais. VoiceStack aceita mono, estéreo e mono para estéreo; as vozes usam uma soma mono e o sinal direto preserva o estéreo.

## Interface 0.4.0

StereoEcho destaca o tempo do delay. SoftCompressor e PeakCeiling têm faders e medidores verticais. SibilanceControl dá mais espaço ao histórico da detecção. DriveTone mantém o painel compacto centrado no Drive. Os IDs VST3 e parâmetros dos cinco efeitos foram preservados.

## Compilar no Windows

Requer Visual Studio 2022 com **Desktop development with C++**, Windows SDK, Git, CMake 3.22 ou superior e conexão para obter o JUCE. Execute na pasta que contém este README.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target StereoEcho_VST3 SoftCompressor_VST3 PeakCeiling_VST3 SibilanceControl_VST3 DriveTone_VST3 CurveTen_VST3 VoiceStack_VST3
```

Os bundles completos ficam em `build/<Efeito>_artefacts/Release/VST3/`. Copie cada pasta `.vst3` inteira para `C:\Program Files\Common Files\VST3`. No FL Studio, abra **Options > Manage plugins**, ative **Verify plugins** e faça a busca. Carregue os efeitos em slots do Mixer. Consulte o [manual de instalação de plugins do FL Studio](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).

## Validação no host

- StereoEcho. Use uma batida curta e renderize com o mix a 100%. Teste 375 ms, sincronização em 1/4 a 120 BPM e a alternância estéreo em ping-pong.
- SoftCompressor. Use um tom de nível constante. Com knee em 0, threshold em −18 dB, ratio 4 e mix a 100%, um pico de −6 dBFS deve estabilizar próximo de −15 dBFS.
- PeakCeiling. Use um sinal com transientes acima do threshold e verifique o teto de pico de amostra. Confira alinhamento com a compensação de latência do Mixer. O teto se aplica a amostras digitais.
- Salve e reabra o projeto do FL Studio para verificar a restauração de parâmetros.

A versão 0.3.0 acrescenta SibilanceControl e DriveTone. Copie os novos bundles para a pasta VST3 e faça uma busca com a verificação de plugins ativada. O modo sem lookahead do SibilanceControl tem latência de processamento declarada de zero. O RAR serviu como referência de inventário e metadados. O processamento é original e o REA não foi executado.
