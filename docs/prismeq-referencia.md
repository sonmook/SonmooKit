# PrismEQ e FabFilter Pro-Q 4

O ponto de partida foi o manual de 64 páginas indicado pelo usuário, junto com a apresentação da fabricante e a ajuda online. O PDF reúne a operação das bandas, interface, dinâmica, edição espectral, análise, modos de fase, integração com o host e presets. Os controles que aparecem no PrismEQ estão descritos no [manual SonmooKit](manuais/PrismEQ.md). [Manual oficial em PDF](https://www.fabfilter.com/downloads/pdf/help/ffproq4-manual.pdf) · [Ajuda online](https://www.fabfilter.com/help/pro-q/intro/fabfilterproq) · [Página do produto](https://www.fabfilter.com/products/pro-q-4-equalizer-plug-in)

| Tema pesquisado | Cobertura no PrismEQ 0.14.0 |
| --- | --- |
| Criação e edição de bandas no gráfico | 24 bandas, arraste, Q pela roda, oito formas, esboço por gesto e seleção de pico próximo. Edição individual e escala fixa. |
| Dinâmica | Detector interno ou externo; processamento de banda e processamento espectral independentes. Implementações próprias de limiar, ataque, release e largura espectral. [Dinâmica espectral do original](https://www.fabfilter.com/help/pro-q/using/spectral-dynamics) |
| Canais | Mono e estéreo, com seleção L/R e M/S por banda. Surround e Atmos ficam fora desta versão. |
| Fase e latência | Live para baixa latência e Spectral ready para o motor espectral. Natural Phase e Linear Phase com resoluções selecionáveis ficam fora desta versão. |
| Análise e referência | Analisador pré ou pós, captura de dois trechos reproduzidos na instância e ajuste tonal em sete regiões. A lista de instâncias, colisões entre pistas e importação direta de arquivos de referência ficam fora desta versão. |
| Ganho, forma e caráter | Três opções próprias de caráter; filtros globais com inclinações discretas. Tilt, Flat Tilt, inclinações fracionárias, Brickwall, Auto Gain e equivalência com as colorações da FabFilter ficam fora desta versão. |
| Fluxo de trabalho e formatos | VST3 x64 para Windows, automação e recuperação de estado pelo host. Seleção múltipla, comparação A/B interna, MIDI Learn e importação de presets FabFilter ficam fora desta versão. |

## Medição direta de uma banda estática

Abrimos o Pro-Q 4 instalado e o PrismEQ em um host VST3. Usamos 48 kHz, blocos de 128 amostras, entrada senoidal a −20 dBFS de pico, estéreo e latência zero. A banda Bell foi configurada em 1 kHz, Q 1 e ganho +6 dB. Para cada frequência, medimos o ganho RMS após um segundo de estabilização. Ajustamos a largura da Bell do PrismEQ com essa medição. Estes pontos são de calibração, não uma validação independente de todas as formas e valores de Q.

| Frequência | Pro-Q 4 | PrismEQ |
| ---: | ---: | ---: |
| 100 Hz | +0,129 dB | +0,129 dB |
| 250 Hz | +0,786 dB | +0,784 dB |
| 500 Hz | +2,830 dB | +2,825 dB |
| 750 Hz | +5,086 dB | +5,083 dB |
| 1 kHz | +6,000 dB | +6,000 dB |
| 1,5 kHz | +4,409 dB | +4,400 dB |
| 2 kHz | +2,826 dB | +2,810 dB |
| 4 kHz | +0,778 dB | +0,753 dB |
| 10 kHz | +0,122 dB | +0,094 dB |

## Verificação do PrismEQ

O verificador carregou o VST3 a 44,1, 48 e 96 kHz com blocos de 63, 128 e 513 amostras, respectivamente. Conferiu a curva plana, ganho de +6 dB apenas à esquerda na banda 24, restauração dos ajustes, dinâmica convencional e latência dos motores.

No ensaio espectral, dois tons simultâneos ficaram dentro da região de análise. A 48 kHz, eles ficaram em 937,5 Hz e 1.406,25 Hz, com amplitudes de 0,4 e 0,008. Com Range em −12 dB e Threshold em −25 dB, o tom forte saiu com 28,93% da amplitude original (cerca de −10,8 dB) e o fraco com 100,00% após arredondamento. Esse ensaio mede a seletividade do PrismEQ. A dinâmica espectral e a saturação ainda não têm uma comparação A/B com os algoritmos da FabFilter. [Código do teste](../tests/PrismCheck.cpp)
