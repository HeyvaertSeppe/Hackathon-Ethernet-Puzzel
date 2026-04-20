Doel van de puzzel: Spelers moeten 5 ethernetstekkers in de juiste (willekeurig gegenereerde) poorten steken nadat de puzzel is geactiveerd door de hoofdcontroller (ESP32).
1. Spelverloop (De 3 Fases)
• Fase 1: WACHTEN
• Status: De puzzel is in rust.
• LCD Scherm: Toont "Los andere puzzels op om te activeren".
• Trigger: Zodra de ESP32 een stroompje stuurt naar de Activate_Pin, genereert het systeem in de achtergrond de oplossing en start het spel.

• Fase 2: SPELEN
• Status: Spelers zijn bezig. De Rode LED knippert.
• LCD Scherm: Toont eerst een welkom, dan instructies, en daarna live feedback (bijv. "2 zijn correct"). Het scherm vertelt niet welke kabels goed zitten.
• Trigger: Zodra de hardware detecteert dat alle 5 kabels in de juiste poorten zitten, is de puzzel opgelost.

• Fase 3: OPGELOST
• Status: Puzzel is gehaald. Rode LED gaat uit, Groene LED gaat aan. De Done_Pin stuurt een signaal naar de ESP32.
• LCD Scherm: Knippert feestelijk "ALLES JUIST!!!".
• Trigger (Auto-Reset): Na precies 30 seconden reset de puzzel zichzelf automatisch en keert terug naar Fase 1.

3. Belangrijke Functies
• Anti-Cheat (Slimme generatie): Wanneer de puzzel start, kijkt het systeem hoe de kabels op dát moment zijn ingeplugd. De nieuwe, willekeurige oplossing wordt zo gekozen dat er op dat startmoment altijd precies 0 kabels correct zitten. Spelers kunnen de puzzel dus nooit per ongeluk oplossen zodra deze aangaat.
• Game Master Monitor (PuTTY):
Als de puzzel met een pc (via USB/UART) is verbonden, kan de Game Master live meekijken. Het systeem print:
• De gegenereerde oplossing bij de start.
• Elke keer dat een speler een kabel erin steekt of uithaalt (en of dit de juiste poort is).

5. Hardware Overzicht
• ESP32 Communicatie:
• Activate_Pin (Input): Ontvangt het startsignaal.
• Done_Pin (Output): Geeft door dat de puzzel is gehaald.
• Sensoren & Feedback:
• Module 0 t/m 4: Lezen uit in welke poort een kabel zit.
• I2C LCD: Adres 0x27, voor hints en instructies.
• LEDs: Rood (spelen) en Groen (opgelost).
