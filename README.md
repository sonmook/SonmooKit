# SonmooKit para FL Studio

Coleção de **20 efeitos VST3 de 64 bits para Windows**. Os plugins rodam nos slots do Mixer do FL Studio, aceitam automação e salvam seus ajustes no projeto.

[Instalação](#instalação) · [Manuais ilustrados](#manuais-ilustrados) · [Série Dial](#série-dial) · [Compilação e testes](docs/compilacao.md) · [Histórico de versões](docs/historico.md)

## Instalação

1. Feche o FL Studio e extraia o pacote VST3 compilado. Se você baixou o código pelo botão **Code → Download ZIP**, siga o [guia de compilação para Windows](docs/compilacao.md) para gerar os plugins.
2. Copie as pastas completas dos efeitos para **SonmooKit** dentro de `C:\Program Files\Common Files\VST3`. Ao final, por exemplo, o equalizador fica em `C:\Program Files\Common Files\VST3\SonmooKit\PrismEQ.vst3`.
3. No FL Studio, abra **Options → Manage plugins**. Marque **Verify plugins** e **Rescan previously verified plugins** e clique em **Find installed plugins**.
4. Abra o **Mixer**, selecione uma pista e escolha o efeito em um slot livre.

**Atualização de uma instalação existente.** Substitua apenas a pasta do efeito que deseja atualizar. Guarde cópias antigas fora dos caminhos de busca do FL Studio para evitar duplicatas. Quem usa automação anterior à versão 0.12.0 pode consultar as [mudanças nas faixas dos parâmetros](docs/revisao-0.12.0.md).

## PrismEQ

Equalizador de 24 bandas com equalização estática, dinâmica de banda e processamento espectral. A versão do código disponível neste repositório é a **0.14.0**. [Manual dessa versão](docs/manuais/PrismEQ.md) · [Comparação com o Pro-Q 4](docs/prismeq-referencia.md)

**No pacote 0.16.6 já distribuído**, selecione uma Bell ou Shelf e escolha **Make Dynamic** ou **Make Spectral**. O controle **GAIN** define o ganho fixo. Arraste verticalmente o marcador de duas setas no gráfico para ajustar **RANGE**, a atuação máxima da banda. Com **GAIN** em 0 dB e **RANGE** em −4 dB, o efeito pode atenuar até 4 dB quando o detector atuar.

## Manuais ilustrados

Clique no nome ou na miniatura para abrir o manual do efeito. A última coluna identifica o plugin usado como referência funcional.

### Equalização

| Painel | Plugin e manual | Função | Referência funcional |
| --- | --- | --- | --- |
| <a href="docs/manuais/PrismEQ.md"><img src="docs/imagens/PrismEQ.png" width="112" alt="Painel do PrismEQ"></a> | [PrismEQ](docs/manuais/PrismEQ.md) | Equalização dinâmica e espectral | FabFilter Pro-Q 4 |
| <a href="docs/manuais/CurveTen.md"><img src="docs/imagens/CurveTen.png" width="112" alt="Painel do CurveTen"></a> | <a id="curveten"></a>[CurveTen](docs/manuais/CurveTen.md) | Equalizador de dez bandas | Q10 |
| <a href="docs/manuais/PulseEQ.md"><img src="docs/imagens/PulseEQ.png" width="112" alt="Painel do PulseEQ"></a> | <a id="pulseeq"></a>[PulseEQ](docs/manuais/PulseEQ.md) | Equalizador dinâmico | F6 |

### Dinâmica e nível

| Painel | Plugin e manual | Função | Referência funcional |
| --- | --- | --- | --- |
| <a href="docs/manuais/SoftCompressor.md"><img src="docs/imagens/SoftCompressor.png" width="112" alt="Painel do SoftCompressor"></a> | <a id="softcompressor"></a>[SoftCompressor](docs/manuais/SoftCompressor.md) | Compressão de dinâmica | Renaissance Compressor |
| <a href="docs/manuais/FETStrike.md"><img src="docs/imagens/FETStrike.png" width="112" alt="Painel do FETStrike"></a> | <a id="fetstrike"></a>[FETStrike](docs/manuais/FETStrike.md) | Compressor FET | CLA-76 |
| <a href="docs/manuais/OptoLevel2A.md"><img src="docs/imagens/OptoLevel2A.png" width="112" alt="Painel do OptoLevel 2A"></a> | <a id="optolevel-2a"></a>[OptoLevel 2A](docs/manuais/OptoLevel2A.md) | Compressor óptico | CLA-2A |
| <a href="docs/manuais/PeakCeiling.md"><img src="docs/imagens/PeakCeiling.png" width="112" alt="Painel do PeakCeiling"></a> | <a id="peakceiling"></a>[PeakCeiling](docs/manuais/PeakCeiling.md) | Limitador de picos | L1 |

### Voz, delay e estéreo

| Painel | Plugin e manual | Função | Referência funcional |
| --- | --- | --- | --- |
| <a href="docs/manuais/SonmooTune.md"><img src="docs/imagens/SonmooTune.png" width="112" alt="Painel do SonmooTune"></a> | <a id="sonmootune"></a>[SonmooTune](docs/manuais/SonmooTune.md) | Correção de afinação vocal | Waves Tune Real-Time |
| <a href="docs/manuais/SibilanceControl.md"><img src="docs/imagens/SibilanceControl.png" width="112" alt="Painel do SibilanceControl"></a> | <a id="sibilancecontrol"></a>[SibilanceControl](docs/manuais/SibilanceControl.md) | Controle de sibilância | Sibilance |
| <a href="docs/manuais/VoiceStack.md"><img src="docs/imagens/VoiceStack.png" width="112" alt="Painel do VoiceStack"></a> | <a id="voicestack"></a>[VoiceStack](docs/manuais/VoiceStack.md) | Dobra de voz | Doubler |
| <a href="docs/manuais/StereoEcho.md"><img src="docs/imagens/StereoEcho.png" width="112" alt="Painel do StereoEcho"></a> | <a id="stereoecho"></a>[StereoEcho](docs/manuais/StereoEcho.md) | Delay sincronizado | H-Delay |
| <a href="docs/manuais/StereoArc.md"><img src="docs/imagens/StereoArc.png" width="112" alt="Painel do StereoArc"></a> | <a id="stereoarc"></a>[StereoArc](docs/manuais/StereoArc.md) | Imagem estéreo | S1 Imager |

### Série Dial

Oito efeitos com um controle principal. Procure por **Dial** no gerenciador do FL Studio. **Dial Drive** é o antigo DriveTone e aparece uma vez no catálogo. [Guia da série e tabela dos nomes antigos](docs/serie-um-controle.md)

| Painel | Plugin e manual | Função | Referência funcional |
| --- | --- | --- | --- |
| <a href="docs/manuais/AirLift.md"><img src="docs/imagens/AirLift.png" width="112" alt="Painel do Dial Bright"></a> | [Dial Bright](docs/manuais/AirLift.md) | Brilho progressivo | OneKnob Brighter |
| <a href="docs/manuais/BassBody.md"><img src="docs/imagens/BassBody.png" width="112" alt="Painel do Dial Bass"></a> | [Dial Bass](docs/manuais/BassBody.md) | Reforço de graves | OneKnob Phatter |
| <a href="docs/manuais/DriveTone.md"><img src="docs/imagens/DriveTone.png" width="112" alt="Painel do Dial Drive"></a> | <a id="drivetone"></a>[Dial Drive](docs/manuais/DriveTone.md) | Saturação | OneKnob Driver |
| <a href="docs/manuais/SweepTone.md"><img src="docs/imagens/SweepTone.png" width="112" alt="Painel do Dial Filter"></a> | [Dial Filter](docs/manuais/SweepTone.md) | Filtro ressonante | OneKnob Filter |
| <a href="docs/manuais/LevelLift.md"><img src="docs/imagens/LevelLift.png" width="112" alt="Painel do Dial Lift"></a> | [Dial Lift](docs/manuais/LevelLift.md) | Elevação de nível | OneKnob Louder |
| <a href="docs/manuais/DrumPress.md"><img src="docs/imagens/DrumPress.png" width="112" alt="Painel do Dial Press"></a> | [Dial Press](docs/manuais/DrumPress.md) | Compressão paralela | OneKnob Pressure |
| <a href="docs/manuais/BeatDip.md"><img src="docs/imagens/BeatDip.png" width="112" alt="Painel do Dial Pump"></a> | [Dial Pump](docs/manuais/BeatDip.md) | Pulsação sincronizada | OneKnob Pumper |
| <a href="docs/manuais/RoomBloom.md"><img src="docs/imagens/RoomBloom.png" width="112" alt="Painel do Dial Verb"></a> | [Dial Verb](docs/manuais/RoomBloom.md) | Ambiência e reverb | OneKnob Wetter |
