''This code example is from Propeller Education Kit Labs: Fundamentals, v1.2.
''A .pdf copy of the book is available from www.parallax.com, and also through
''the Propeller Tool software's Help menu (v1.2.6 or newer).
''
''DoReMi.spin
''Play C6, D6, E6, F6, G6, A6, B6, C7 as quarter notes quarter stops between.

CON
   
  _clkmode = xtal1 + pll16x                  ' System clock → 80 MHz
  _xinfreq = 5_000_000

PUB TestFrequency | index

  'Configure ctra module 
  ctra[30..26] := %00100                     ' Set ctra for "NCO single-ended"
  ctra[5..0] := 27                           ' Set APIN to P27
  frqa := 0                                  ' Don't play any notes yet

  repeat index from 0 to 7

    
    frqa := long[@notes][index]              'Set the frequency.
                                     
    'Broadcast the signal for 1/4 s
    dira[27]~~                               ' Set P27 to output
    waitcnt(clkfreq/4 + cnt)                 ' Wait for tone to play for 1/4 s

    
    dira[27]~                                '1/4 s stop
    waitcnt(clkfreq/4 + cnt)

DAT
'80 MHz frqa values for square wave musical note approximations with the counter module
'configured to NCO:
'          C6      D6      E6      F6      G6      A6      B6       C7
notes long 56_184, 63_066, 70_786, 74_995, 84_181, 94_489, 105_629, 112_528