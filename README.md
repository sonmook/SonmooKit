# SonmooKit para FL Studio

Projeto C++ com JUCE 8.0.6. Gera dez efeitos VST3 x64 independentes, com interface própria, medidores em tempo real, parâmetros automatizáveis e restauração de estado no projeto.

| Efeito | Referência funcional | Controles |
| --- | --- | --- |
| StereoEcho | H-Delay | Tempo em milissegundos ou sincronizado ao BPM do host, feedback, filtros, ping-pong e mix |
| SoftCompressor | Renaissance Compressor | Threshold, ratio, ataque, release, knee, makeup e mix paralelo. O detector de pico aplica o mesmo ganho aos dois canais |
| SibilanceControl | Sibilance | Limiar, faixa de redução, largura da detecção, transição Wide/Split, audição do sinal removido, lookahead de 12 ms e histórico do detector |
| DriveTone | OneKnob Driver | Drive de 0 a 10, tonalidade, ajuste de saída e mix |
| PeakCeiling | L1 | Threshold, teto de pico de amostra em dBFS, release e lookahead fixo de 2 ms com latência reportada ao host |
| CurveTen | Q10 | Dez bandas, seis tipos de filtro, curva arrastável, espectro de saída e ganho global |
| VoiceStack | Doubler | Quatro vozes com ganho, pan, delay, detune, feedback, oitava e modulação por voz |
| PulseEQ | F6 | Seis bandas dinâmicas, cortes, analisador e sidechain por banda |
| FETStrike | CLA-76 | Compressor FET com Input, Output, tempos, quatro ratios, All e medidor de agulha |
| PitchPilot | Waves Tune Real-Time | Correção vocal por escala ou MIDI, tolerância, transições, vibrato, referência e teclado interativo |

## PitchPilot 0.8.0

Afinador de voz monofônica com entrada mono ou estéreo. Escolha a extensão vocal, a tônica e a escala. **Speed** ajusta a velocidade da correção e **Transition** suaviza mudanças de nota-alvo. **Tolerance** exige uma margem em cents e um tempo mínimo antes da troca. **Link times** usa o tempo de Speed para as transições. O visor mostra a nota de entrada e seu desvio em relação ao alvo. No teclado, verde indica o alvo e o ponto coral indica a entrada.

Em **Edit scale**, clique em uma tecla para incluir ou excluir aquela classe de nota em todas as oitavas. Desative **Group octaves** para editar só a oitava clicada. O nível de **Reference tone** é ajustável em dB. **Reset keys** restaura a escala. Em **Reference tone**, segure uma tecla para ouvir a referência. Em **Hold target**, segure a nota à qual deseja afinar. Para controlar pelo Piano Roll, envie as notas com MIDI Out para a porta MIDI de entrada do wrapper e ative **MIDI target**. Ao soltar todas as notas, a escala volta a determinar o alvo.

A correção usa detecção de periodicidade e ressíntese granular com preservação opcional de formantes. O controle Vibrato regula os desvios rápidos em torno da tendência de afinação. A latência declarada é de 45 ms. Use em uma voz isolada. O DSP é próprio. A [documentação da Waves sobre correção vocal](https://www.waves.com/how-to-correct-vocal-pitch-more-naturally) orientou o fluxo de uso.

## FETStrike 0.7.0

Compressor estéreo vinculado, com dois caracteres, ruído analógico opcional, mix paralelo, ajuste de saída e medição GR/IN/OUT. Attack e Release vão de 1 (mais lento) a 7 (mais rápido). O modelo de compressão é próprio.

## Versão 0.6.2

Os oito plugins usam a logo SonmooKit nos cabeçalhos, com uma marca vetorial compartilhada.

## PulseEQ 0.6.1

O analisador atualiza a interface a até 60 Hz e usa janelas FFT de 4096 amostras sobrepostas. O traço decai suavemente entre quadros.

## PulseEQ 0.6.0

Oito filtros incluem seis bandas dinâmicas e dois cortes globais. Cada banda tem ganho estático, range dinâmico, limiar, ataque e release, com Stereo/Mid/Side e detector Split/Wide. O sidechain externo usa o bus auxiliar VST3. O espectro mostra a saída e o gráfico compara a curva estática com a curva dinâmica. A resposta do processamento é própria.

## CurveTen 0.5.2

O painel completo mostra as dez bandas e os controles de ganho, frequência, Q e filtro simultaneamente. No gráfico, arraste para editar frequência e ganho, use Alt para ajustar Q e dê duplo clique para alternar a banda. Há faders de entrada e inversão de polaridade por canal, ganho de saída comum e medição estéreo.

## VoiceStack 0.5.1

As quatro vozes ficam visíveis ao mesmo tempo. Os gráficos permitem arrastar panorama e ganho, delay e detune ou frequência e ganho das prateleiras do sinal das vozes. Os controles globais incluem alinhamento do sinal direto, faixa de afinação estendida e reinício da modulação.

## Versão 0.5.0

CurveTen e VoiceStack são efeitos com algoritmos próprios. CurveTen aplica as mesmas bandas aos dois canais. VoiceStack aceita mono, estéreo e mono para estéreo; as vozes usam uma soma mono e o sinal direto preserva o estéreo.

## Interface 0.4.0

StereoEcho destaca o tempo do delay. SoftCompressor e PeakCeiling têm faders e medidores verticais. SibilanceControl dá mais espaço ao histórico da detecção. DriveTone mantém o painel compacto centrado no Drive. Os IDs VST3 e parâmetros dos cinco efeitos foram preservados.

## Compilar no Windows

Requer Visual Studio 2022 com **Desktop development with C++**, Windows SDK, Git, CMake 3.22 ou superior e conexão para obter o JUCE. Execute na pasta que contém este README.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target StereoEcho_VST3 SoftCompressor_VST3 PeakCeiling_VST3 SibilanceControl_VST3 DriveTone_VST3 CurveTen_VST3 VoiceStack_VST3 PulseEQ_VST3 FETStrike_VST3 PitchPilot_VST3
```

Os bundles completos ficam em `build/<Efeito>_artefacts/Release/VST3/`. Copie cada pasta `.vst3` inteira para `C:\Program Files\Common Files\VST3`. No FL Studio, abra **Options > Manage plugins**, ative **Verify plugins** e faça a busca. Carregue os efeitos em slots do Mixer. Consulte o [manual de instalação de plugins do FL Studio](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).

## Validação no host

- StereoEcho. Use uma batida curta e renderize com o mix a 100%. Teste 375 ms, sincronização em 1/4 a 120 BPM e a alternância estéreo em ping-pong.
- SoftCompressor. Use um tom de nível constante. Com knee em 0, threshold em −18 dB, ratio 4 e mix a 100%, um pico de −6 dBFS deve estabilizar próximo de −15 dBFS.
- PeakCeiling. Use um sinal com transientes acima do threshold e verifique o teto de pico de amostra. Confira alinhamento com a compensação de latência do Mixer. O teto se aplica a amostras digitais.
- Salve e reabra o projeto do FL Studio para verificar a restauração de parâmetros.

A versão 0.3.0 acrescenta SibilanceControl e DriveTone. Copie os novos bundles para a pasta VST3 e faça uma busca com a verificação de plugins ativada. O modo sem lookahead do SibilanceControl tem latência de processamento declarada de zero. O RAR serviu como referência de inventário e metadados. O processamento é original e o REA não foi executado.
