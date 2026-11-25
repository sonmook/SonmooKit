# Série de um controle · SonmooKit 0.13.0

O pacote da série reúne sete novos efeitos e o DriveTone 0.12.0, já presente na coleção. Carregue cada VST3 em um **insert do Mixer**. O controle principal vai de 0 a 10. SweepTone abre em 10, com o filtro totalmente aberto. Os demais começam em zero. No DriveTone, mantenha Tone e Output Trim em zero e Mix em 100% para trabalhar só com Drive.

| Efeito SonmooKit | Referência | Função da macro |
| --- | --- | --- |
| [AirLift](manuais/AirLift.md) | [Brighter](https://www.waves.com/plugins/oneknob-brighter) | Brilho progressivo |
| [BassBody](manuais/BassBody.md) | [Phatter](https://www.waves.com/plugins/oneknob-phatter) | Reforço de graves |
| [SweepTone](manuais/SweepTone.md) | [Filter](https://www.waves.com/plugins/oneknob-filter) | Filtro ressonante |
| [DrumPress](manuais/DrumPress.md) | [Pressure](https://www.waves.com/plugins/oneknob-pressure) | Compressão paralela |
| [LevelLift](manuais/LevelLift.md) | [Louder](https://www.waves.com/plugins/oneknob-louder) | Elevação de nível |
| [BeatDip](manuais/BeatDip.md) | [Pumper](https://www.waves.com/plugins/oneknob-pumper) | Pulsação sincronizada |
| [RoomBloom](manuais/RoomBloom.md) | [Wetter](https://www.waves.com/plugins/oneknob-wetter) | Ambiência e reverb |
| [DriveTone](manuais/DriveTone.md) | [Driver](https://www.waves.com/plugins/oneknob-driver) | Overdrive com filtragem dos agudos |

## Documentação consultada

O guia geral da Waves descreve Brighter, Phatter, Filter, Pressure, Louder, Driver e Wetter. O Pumper tem um guia próprio sobre o oscilador, as divisões musicais e o deslocamento de fase. As páginas individuais estão na tabela acima. [Manual da OneKnob Series (PDF)](https://assets.wavescdn.com/pdf/plugins/oneknob-series.pdf) · [Manual do OneKnob Pumper (PDF)](https://assets.wavescdn.com/pdf/plugins/oneknob-pumper.pdf)

Na instalação Windows V17, os oito bundles OKBrighter, OKPhatter, OKFilter, OKPressure, OKLouder, OKDriver, OKWetter e OKPumper incluem metadados de parâmetros e de interface. Foram conferidos o seletor de quatro estados do filtro, o seletor de três estados do compressor e as listas de Rate e Offset do Pumper. Os binários, gráficos e manuais da Waves permanecem na distribuição da fabricante.

## Correspondência sonora

Os controles e o uso musical seguem as referências. As curvas de equalização e compressão, saturação, modulação e reverberação são implementações próprias. Ganhos, tempos e medidas indicados nos manuais SonmooKit correspondem a estes VST3. A documentação pública da Waves descreve a operação, mas não fornece uma especificação amostra a amostra.

## Verificação dos VST3

Os oito efeitos passaram por 24 testes, combinando 44,1 kHz com blocos de 63 amostras, 48 kHz com 128 e 96 kHz com 513. Em cada configuração conferimos o sinal na posição neutra, a recuperação do ajuste salvo e o processamento com a macro ativa. A 48 kHz, com o controle no máximo, o AirLift acrescentou cerca de 13,4 dB a 12 kHz e o BassBody acrescentou 13,7 dB a 60 Hz. O LevelLift chegou a 24 dB de ganho no sinal fraco. Também verificamos a ressonância do SweepTone, os níveis de entrada do DrumPress, a saturação do DriveTone, a sincronização do BeatDip e a cauda estéreo do RoomBloom com entrada mono.

Os testes usaram um host VST3 de bancada. A tentativa de comparação direta com a Waves V17 parou na inicialização do WaveShell, antes de obter áudio de referência. As medidas acima descrevem apenas a série SonmooKit.
