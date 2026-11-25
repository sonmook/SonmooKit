# SonmooKit para FL Studio

Coleção de **19 efeitos VST3 x64 para Windows**, com controles automatizáveis e ajustes salvos no projeto. A coleção base 0.12.1 reúne 12 efeitos. O pacote Série de um controle 0.13.0 acrescenta sete efeitos e inclui o DriveTone já presente na base. Os dois pacotes usam a pasta `SonmooKit`.

[Série de um controle](docs/serie-um-controle.md) · [Instalação](#instalação) · [Manuais ilustrados](#manuais-ilustrados) · [Compilação e testes](docs/compilacao.md) · [Histórico de versões](docs/historico.md)

## Instalação

1. Feche o FL Studio e extraia o ZIP **da coleção compilada**. Copie a pasta **SonmooKit** inteira para `C:\Program Files\Common Files\VST3`. O delay, por exemplo, ficará em `VST3\SonmooKit\StereoEcho.vst3`.
2. Abra **Options → Manage plugins** no FL Studio. Marque **Verify plugins** e **Rescan previously verified plugins** e clique em **Find installed plugins**.
3. No **Mixer**, escolha a pista que deseja processar e carregue o efeito em um slot.

**Se já usa a coleção**, guarde as cópias antigas em uma pasta de backup fora dos caminhos pesquisados pelo FL Studio antes de instalar. Algumas faixas de parâmetros mudaram na versão 0.12.0. Em projetos com automação, compare o resultado com a renderização anterior. [Mudanças e testes de áudio](docs/revisao-0.12.0.md)

## Manuais ilustrados

Clique no nome ou na imagem para abrir o manual. Os controles aparecem com os nomes em inglês que você encontra no painel.

| Painel | Manual | Função | Referência funcional |
| --- | --- | --- | --- |
| <a href="docs/manuais/StereoEcho.md"><img src="docs/imagens/StereoEcho.png" width="112" alt="Painel do StereoEcho"></a> | <a id="stereoecho"></a>[StereoEcho](docs/manuais/StereoEcho.md) | Delay sincronizado | H-Delay |
| <a href="docs/manuais/SoftCompressor.md"><img src="docs/imagens/SoftCompressor.png" width="112" alt="Painel do SoftCompressor"></a> | <a id="softcompressor"></a>[SoftCompressor](docs/manuais/SoftCompressor.md) | Compressão de dinâmica | Renaissance Compressor |
| <a href="docs/manuais/PeakCeiling.md"><img src="docs/imagens/PeakCeiling.png" width="112" alt="Painel do PeakCeiling"></a> | <a id="peakceiling"></a>[PeakCeiling](docs/manuais/PeakCeiling.md) | Limitador de picos | L1 |
| <a href="docs/manuais/SibilanceControl.md"><img src="docs/imagens/SibilanceControl.png" width="112" alt="Painel do SibilanceControl"></a> | <a id="sibilancecontrol"></a>[SibilanceControl](docs/manuais/SibilanceControl.md) | Controle de sibilância | Sibilance |
| <a href="docs/manuais/DriveTone.md"><img src="docs/imagens/DriveTone.png" width="112" alt="Painel do DriveTone"></a> | <a id="drivetone"></a>[DriveTone](docs/manuais/DriveTone.md) | Saturação | OneKnob Driver |
| <a href="docs/manuais/CurveTen.md"><img src="docs/imagens/CurveTen.png" width="112" alt="Painel do CurveTen"></a> | <a id="curveten"></a>[CurveTen](docs/manuais/CurveTen.md) | Equalizador de dez bandas | Q10 |
| <a href="docs/manuais/VoiceStack.md"><img src="docs/imagens/VoiceStack.png" width="112" alt="Painel do VoiceStack"></a> | <a id="voicestack"></a>[VoiceStack](docs/manuais/VoiceStack.md) | Dobra de voz | Doubler |
| <a href="docs/manuais/PulseEQ.md"><img src="docs/imagens/PulseEQ.png" width="112" alt="Painel do PulseEQ"></a> | <a id="pulseeq"></a>[PulseEQ](docs/manuais/PulseEQ.md) | Equalizador dinâmico | F6 |
| <a href="docs/manuais/FETStrike.md"><img src="docs/imagens/FETStrike.png" width="112" alt="Painel do FETStrike"></a> | <a id="fetstrike"></a>[FETStrike](docs/manuais/FETStrike.md) | Compressor FET | CLA-76 |
| <a href="docs/manuais/OptoLevel2A.md"><img src="docs/imagens/OptoLevel2A.png" width="112" alt="Painel do OptoLevel 2A"></a> | <a id="optolevel-2a"></a>[OptoLevel 2A](docs/manuais/OptoLevel2A.md) | Compressor óptico | CLA-2A |
| <a href="docs/manuais/SonmooTune.md"><img src="docs/imagens/SonmooTune.png" width="112" alt="Painel do SonmooTune"></a> | <a id="sonmootune"></a>[SonmooTune](docs/manuais/SonmooTune.md) | Correção de afinação vocal | Waves Tune Real-Time |
| <a href="docs/manuais/StereoArc.md"><img src="docs/imagens/StereoArc.png" width="112" alt="Painel do StereoArc"></a> | <a id="stereoarc"></a>[StereoArc](docs/manuais/StereoArc.md) | Imagem estéreo | S1 Imager |
| <a href="docs/manuais/AirLift.md"><img src="docs/imagens/AirLift.png" width="112" alt="Painel do AirLift"></a> | [AirLift](docs/manuais/AirLift.md) | Brilho progressivo | OneKnob Brighter |
| <a href="docs/manuais/BassBody.md"><img src="docs/imagens/BassBody.png" width="112" alt="Painel do BassBody"></a> | [BassBody](docs/manuais/BassBody.md) | Reforço de graves | OneKnob Phatter |
| <a href="docs/manuais/SweepTone.md"><img src="docs/imagens/SweepTone.png" width="112" alt="Painel do SweepTone"></a> | [SweepTone](docs/manuais/SweepTone.md) | Filtro ressonante | OneKnob Filter |
| <a href="docs/manuais/DrumPress.md"><img src="docs/imagens/DrumPress.png" width="112" alt="Painel do DrumPress"></a> | [DrumPress](docs/manuais/DrumPress.md) | Compressão paralela | OneKnob Pressure |
| <a href="docs/manuais/LevelLift.md"><img src="docs/imagens/LevelLift.png" width="112" alt="Painel do LevelLift"></a> | [LevelLift](docs/manuais/LevelLift.md) | Elevação de nível | OneKnob Louder |
| <a href="docs/manuais/BeatDip.md"><img src="docs/imagens/BeatDip.png" width="112" alt="Painel do BeatDip"></a> | [BeatDip](docs/manuais/BeatDip.md) | Pulsação sincronizada | OneKnob Pumper |
| <a href="docs/manuais/RoomBloom.md"><img src="docs/imagens/RoomBloom.png" width="112" alt="Painel do RoomBloom"></a> | [RoomBloom](docs/manuais/RoomBloom.md) | Ambiência e reverb | OneKnob Wetter |
