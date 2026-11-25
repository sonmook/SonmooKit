# Revisão de funcionamento · 0.12.0

A comparação usa os dez manuais da Waves, a lista de parâmetros dos dez bundles V17 instalados, a implementação dos VST3 SonmooKit e testes de áudio dos plugins compilados. A coluna de diferenças aponta o que ainda impede usar os dois produtos como substitutos exatos. Os valores de exemplo e as medidas da bancada se referem à SonmooKit.

| SonmooKit | Comportamento da versão 0.12.0 | Diferenças e verificações pendentes | Manual Waves |
| --- | --- | --- | --- |
| StereoEcho | Tempo livre de 1 a 3500 ms. Dois cliques em Tap definem o intervalo. Com sincronismo ativo, o plugin escolhe a divisão disponível mais próxima. Modulação de 0,1 a 6000 Hz, LoFi, saída ±18 dB e feedback até 200%. | Modos Analog, tempos independentes do modo Dual, sincronização com BPM digitado e a coleção completa de divisões do H-Delay. | [H-Delay](https://assets.wavescdn.com/pdf/plugins/h-delay-hybrid-delay.pdf) |
| SoftCompressor | Ratio de 0,5:1 a 50:1, ataque de 0,5 a 500 ms, release de 5 a 5000 ms e compensação de −30 a +30 dB. Abaixo de 1:1 há expansão. Adaptive release usa o histórico do detector para controlar a recuperação. | Modos Electro/Opto e Warm/Smooth, limitador de saída e calibração da envoltória ARC. | [Renaissance Compressor](https://assets.wavescdn.com/pdf/plugins/renaissance-compressor.pdf) |
| SibilanceControl | Audition reproduz os trechos identificados pelo detector, inclusive com Range em zero. O modo Split concentra a redução acima de 4 kHz. Lookahead acrescenta 12 ms. | Classificação de consoantes em voz real e comportamento do detector proprietário. | [Sibilance](https://assets.wavescdn.com/pdf/plugins/sibilance.pdf) |
| DriveTone | A saturação agora calcula a média da curva entre amostras para reduzir o rebatimento de harmônicos. O ajuste zero continua transparente com trim neutro. | Curva de saturação, espectro harmônico e compensação de volume para cada posição de Drive. | [OneKnob Driver](https://assets.wavescdn.com/pdf/plugins/oneknob-series.pdf) |
| PeakCeiling | Release manual de 0,01 a 1000 ms. Adaptive release reage à duração e à intensidade da limitação. O teto é expresso em picos de amostra. | True Peak, IDR, dither e calibração da recuperação automática da referência. | [L1](https://assets.wavescdn.com/pdf/plugins/l1-ultramaximizer.pdf) |
| CurveTen | Os filtros de corte têm inclinação de 12 dB/oitava e resposta de −3 dB na frequência selecionada, inclusive quando uma banda antes usada como Bell tem Q alto. Linked e edição independente L/R oferecem duas curvas de dez bandas. | Modos de precisão e curvas medidas dos demais filtros. | [Q10](https://assets.wavescdn.com/pdf/plugins/q10-equalizer.pdf) |
| VoiceStack | Em estéreo, as vozes 1 e 3 recebem o canal esquerdo, e 2 e 4 recebem o direito. A referência de alinhamento das vozes é de 7 ms na faixa normal e 24 ms na estendida. Os seletores IN 1/3 e IN 2/4 também aceitam Left, Right e L+R. O passa-altas do feedback vai de 16 Hz a 12 kHz. | Textura da transposição e resposta do feedback nas quatro vozes. | [Doubler](https://assets.wavescdn.com/pdf/plugins/doubler.pdf) |
| PulseEQ | Os cortes HP e LP oferecem 6, 12, 18 e 24 dB/oitava. A curva exibida usa os mesmos filtros do áudio. O espectro permite escolher a soma estéreo normalizada (L+R)/2, esquerda, direita ou componente lateral. | Outras fontes e opções de visualização do RTA, vínculo de controles entre bandas e resposta exata do detector. | [F6](https://assets.wavescdn.com/pdf/plugins/f6.pdf) |
| FETStrike | A escala de ataque vai de 1 ms a 25 µs e a de release nominal vai de 1 s a 50 ms. O modo All e os dois caracteres continuam com curvas próprias. | Distorção e recuperação por nível e revisão, especialmente em All. A escala de ganho de entrada e saída também difere. | [CLA-76](https://assets.wavescdn.com/pdf/plugins/cla-76-compressor-limiter-v16-update.pdf) |
| SonmooTune | O detector verifica a periodicidade acima da extensão selecionada para evitar afinar um harmônico como se estivesse dentro dela. O bypass personalizado considera a nota cantada. | Transferência de escalas entre instâncias e equivalência de timbre, formantes, vibrato e transições em tomadas vocais. | [Waves Tune Real-Time](https://assets.wavescdn.com/pdf/plugins/tune-real-time.pdf) |

## Resultados no Windows

O host abriu separadamente os dez VST3 x64 e processou blocos estéreo de 128 amostras a 48 kHz. Os 16 cenários passaram. Alguns resultados que ajudam a reproduzir os testes seguem abaixo.

| Teste | Resultado medido |
| --- | --- |
| StereoEcho, impulso, 3500 ms, mix 100% | Primeiro eco na amostra 168000. |
| SoftCompressor, seno de 1 kHz com pico de −12,04 dBFS, limiar −18 dB, ratio 0,5 e knee 0 | Ganho estabilizado de 5,81 dB. |
| SibilanceControl, seno de 7,5 kHz, monitor ativo e Range em zero | Sinal detectado audível com ganho unitário em regime. |
| DriveTone, seno de 1 kHz, Drive 0 e trim neutro | Razão de níveis RMS de 1,000. O ensaio com Drive 8 também produziu saída finita. |
| PeakCeiling, seno de 1 kHz, teto −12 dBFS | Pico de 0,251189, equivalente a −12,00 dBFS. Latência declarada de 96 amostras. |
| CurveTen, passa-baixas em 1 kHz | −3,01 dB na frequência de corte. Em teste separado, a banda direita em +6 dB manteve a esquerda em 0 dB. |
| VoiceStack, impulso em oposição de fase, voz 1 à esquerda, delay de 10 ms, detune zero | Primeira amostra em 17 ms na faixa normal e 34 ms na estendida. Selecionar L+R cancelou o sinal de teste. |
| PulseEQ, banda em 1 kHz, range −9 dB | Atenuação de 9,00 dB no tom sustentado. Um passa-baixas de 500 Hz com inclinação de 24 dB/oitava atenuou 1 kHz em 24,14 dB. |
| FETStrike, seno de 1 kHz com pico de −12,04 dBFS e Input +12 dB | Razão RMS saída/entrada de 0,849. A medida combina compressão e saturação do modelo. |
| SonmooTune, cromático e referência de 440 Hz | Entrada de 451 Hz corrigida para 439,88 Hz. Em Bass, a entrada de 660 Hz passou a 660 Hz. |

## Projetos existentes

Os identificadores VST3 foram mantidos. A revisão altera o som de alguns ajustes, particularmente a alimentação estéreo das vozes e o Q nos filtros de corte. Nos parâmetros cuja faixa mudou, clipes de automação gravados em valores normalizados podem produzir outro valor físico. Para comparar versões, guarde a renderização e os VST3 anteriores.

## Comparação de áudio com a Waves

A tentativa de carregar os WaveShells VST3 17.1 e 17.2 no host de teste do Windows terminou sem enumerar os efeitos. A segunda tentativa com o 17.1 atingiu 120 segundos. Isso impediu medir a diferença de áudio entre as duas marcas no mesmo sinal e ajuste.
