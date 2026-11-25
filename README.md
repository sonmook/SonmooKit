# SonmooKit para FL Studio

Projeto C++ com JUCE 8.0.6. Gera doze efeitos VST3 x64 independentes, com interface própria, medidores em tempo real, parâmetros automatizáveis e restauração de estado no projeto.

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
| OptoLevel 2A | CLA-2A | Gain, Peak Reduction, Compress/Limit, HiFreq, medidor VU, Analog, Auto makeup, Mix e Trim |
| StereoArc | S1 Imager | Largura, rotação, assimetria, ganho, entrada L/R ou M/S, polaridade, troca de canais e medidores de pico |
| SonmooTune | Waves Tune Real-Time | Correção vocal por escala ou MIDI, tolerância, transições, vibrato, referência e teclado interativo |

## StereoArc 0.11.0

Processador de imagem estéreo com entrada e saída de dois canais. Em **Width**, 0 soma os canais em mono, 1 preserva a largura original e 3 triplica a componente lateral. **Rotation** desloca a imagem inteira. **Asymmetry** altera as laterais e mantém uma fonte mono no centro.

Arraste no gráfico para ajustar rotação e ganho. Com **Alt**, o mesmo gesto ajusta assimetria e largura. O gráfico representa a configuração da imagem. Os medidores mostram os picos do áudio em L/R ou M/S. Use **Reset image** para centralizar e restaurar a largura original.

Implementação própria baseada nos controles descritos no [manual Waves S1](https://assets.wavescdn.com/pdf/plugins/s1-stereo-imager.pdf).

## OptoLevel 2A 0.10.1

Compressor óptico mono ou estéreo, com detecção vinculada entre os canais e zero amostras de latência declarada. Aumente **Peak Reduction** para comprimir e use **Gain** para acertar o volume. O ponto de referência de ganho unitário é 32,28. Em **Compress**, a curva é mais suave. **Limit** aplica mais redução e ainda permite a passagem dos transientes durante o ataque.

A envoltória tem ataque nominal de 10 ms e dois estágios de recuperação. O estágio lento acumula redução durante sons fortes e sustentados. **HiFreq** em 100 mantém o detector plano. Girar o controle para a esquerda reduz a influência dos graves sobre a compressão. **VU Display** alterna entre entrada, redução e saída. Os medidores de nível usam uma senoide de pico −18 dBFS como referência para 0 VU. **Analog** seleciona ruído e hum de 50 ou 60 Hz, **Auto makeup** compensa parte da perda prevista, **Mix** permite compressão paralela e **Trim** atua sobre a saída final.

No teste a 48 kHz, uma senoide de 1 kHz com pico de −6,02 dBFS, Gain em 32,28, Peak Reduction em 65, HiFreq em 100 e Analog e Auto makeup desligados resultou em 14,41 dB de atenuação em Compress e 20,43 dB em Limit. Em Compress, 300 ms depois de reduzir o estímulo a −60 dBFS, a atenuação residual foi de aproximadamente 1,50 dB após 100 ms de sinal forte e 6,04 dB após três segundos. São medidas deste DSP original.

Referências de projeto: [controles do Waves CLA-2A](https://assets.wavescdn.com/pdf/plugins/cla-2a-compressor-limiter-v16-update.pdf) e [funcionamento da célula óptica do Teletronix LA-2A](https://media.uaudio.com/assetlibrary/l/a/la-2a_manual.pdf).

## SonmooTune 0.9.0

O afinador anteriormente chamado PitchPilot passa a aparecer como SonmooTune. O alvo de compilação continua sendo `PitchPilot_VST3`.

Afinador de voz monofônica com entrada mono ou estéreo. Escolha a extensão vocal, a tônica e a escala. **Speed** ajusta a velocidade da correção e **Transition** suaviza mudanças de nota-alvo. Com as duas tolerâncias ativas, a primeira condição atingida (desvio em cents ou tempo fora da nota) libera a troca. **Link times** usa o tempo de Speed para as transições. O visor mostra o alvo, a correção em cents calculada a partir da razão aplicada e a nota detectada. No teclado, verde indica o alvo e o ponto coral indica a entrada.

Em **Edit scale**, clique em uma tecla para incluir ou excluir aquela classe de nota em todas as oitavas. Clique com o botão direito para escolher correção para cima, para baixo ou bypass da nota. Um alvo MIDI pode substituir essa regra. Desative **Group octaves** para editar só a oitava clicada. O nível de **Reference tone** é ajustável em dB. **Reset keys** restaura a escala. Em **Reference tone**, segure uma tecla para ouvir a referência. Em **Hold target**, segure a nota à qual deseja afinar. Para controlar pelo Piano Roll, envie as notas com MIDI Out para a porta MIDI de entrada do wrapper e ative **MIDI target**. Ao soltar todas as notas, a escala volta a determinar o alvo.

A correção usa detecção de periodicidade e ressíntese granular com preservação opcional de formantes. O controle Vibrato acompanha oscilações recorrentes de afinação. O sinal direto passa sem o atraso fixo de 45 ms da versão 0.8.1. O VST3 declara zero amostras de latência fixa. A detecção e os grãos de correção têm resposta temporal dependente da nota. Use em uma voz isolada. O DSP é próprio. O [manual do Waves Tune Real-Time](https://assets.wavescdn.com/pdf/plugins/tune-real-time-v16-update.pdf) orientou o fluxo de uso. Em bancada a 48 kHz, uma senoide de 451 Hz saiu a 439,88 Hz com correção total e a 445,40 Hz com correção de 50%. A equivalência sonora com o Waves em gravações vocais ainda precisa de comparação direta.

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
cmake --build build --config Release --target StereoEcho_VST3 SoftCompressor_VST3 PeakCeiling_VST3 SibilanceControl_VST3 DriveTone_VST3 CurveTen_VST3 VoiceStack_VST3 PulseEQ_VST3 FETStrike_VST3 PitchPilot_VST3 OptoLevel2A_VST3 StereoArc_VST3
```

Os bundles completos ficam em `build/<Efeito>_artefacts/Release/VST3/`. Copie cada pasta `.vst3` inteira para `C:\Program Files\Common Files\VST3`. No FL Studio, abra **Options > Manage plugins**, ative **Verify plugins** e faça a busca. Carregue os efeitos em slots do Mixer. Consulte o [manual de instalação de plugins do FL Studio](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).

## Validação no host

- StereoEcho. Use uma batida curta e renderize com o mix a 100%. Teste 375 ms, sincronização em 1/4 a 120 BPM e a alternância estéreo em ping-pong.
- SoftCompressor. Use um tom de nível constante. Com knee em 0, threshold em −18 dB, ratio 4 e mix a 100%, um pico de −6 dBFS deve estabilizar próximo de −15 dBFS.
- PeakCeiling. Use um sinal com transientes acima do threshold e verifique o teto de pico de amostra. Confira alinhamento com a compensação de latência do Mixer. O teto se aplica a amostras digitais.
- Salve e reabra o projeto do FL Studio para verificar a restauração de parâmetros.

A versão 0.3.0 acrescenta SibilanceControl e DriveTone. Copie os novos bundles para a pasta VST3 e faça uma busca com a verificação de plugins ativada. O modo sem lookahead do SibilanceControl tem latência de processamento declarada de zero. O RAR serviu como referência de inventário e metadados. O processamento é original e o REA não foi executado.
