''This schematic example is from Propeller Education Kit Labs: Fundamentals, v1.2.
''A .pdf copy of the book is available from www.parallax.com, and also through
''the Propeller Tool software's Help menu (v1.2.6 or newer).
''
{{
 SCHEMATIC LEDs and Pushbuttons
 ────────────────────────────────────────────────────────────────────────────────────────────
 Lab Schematic for:

   • I/O and Timing
   • Methods and Cogs
   • Objects
   
  LED SCHEMATIC           Pushbuttons                                                    
 ──────────────────────   ───────────────────────────────────────────────────────────────────

       (all)                      3.3 V                  3.3 V                  3.3 V        
       100 Ω  LED                                                                         
  P4 ──────────┐               │                      │                      │          
                    │              ┤Pushbutton           ┤Pushbutton           ┤Pushbutton
  P5 ──────────┫               │                      │                      │          
                    │     P21 ───┫            P22 ───┫            P23 ───┫          
  P6 ──────────┫          100 Ω│                 100 Ω│                 100 Ω│          
                    │               │                      │                      │       
  P7 ──────────┫                10 kΩ                 10 kΩ                 10 kΩ    
                    │               │                      │                      │          
  P8 ──────────┫                                                                     
                    │              GND                    GND                    GND        
  P9 ──────────┫     
                         
                   GND    

 ────────────────────────────────────────────────────────────────────────────────────────────
}}
PUB EmptyMethod                                                                                                  