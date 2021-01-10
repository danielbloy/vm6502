@echo off
set test=cmake-build-debug\vm6502.exe --hexfile=hexfiles\tests\

echo EXPECT/ACTUAL error-1.hex
echo SYNTAX ERROR at line 3: Unexpected string before colon - 0dh6
%test%error-1.hex

echo EXPECT/ACTUAL error-2.hex
echo SYNTAX ERROR at line 4: Not a valid hexadecimal number - FH
%test%error-2.hex

echo EXPECT/ACTUAL error-3.hex
echo SYNTAX ERROR at line 3: The value is not a valid hexadecimal number! -  80G0
%test%error-3.hex

echo EXPECT/ACTUAL error-4.hex
echo SYNTAX ERROR at line 3: The value is not a valid hexadecimal number! -  A2Q00
%test%error-4.hex

echo EXPECT/ACTUAL error-5.hex
echo SYNTAX ERROR at line 3: The value is not a valid hexadecimal number! -  70G0CH
%test%error-5.hex

echo EXPECT/ACTUAL error-6.hex
echo ERROR: The value 'S200' specified for the address in the watch ' S200' is not a valid hexadecimal number!
echo SYNTAX ERROR at line 4: Invalid watch definition - WATCH: S200
%test%error-6.hex

echo EXPECT/ACTUAL error-7.hex
echo ERROR: The value 'G' specified for the type in the watch ' 200-G' is not one of a, A, c, C, h, H, s or S!
echo SYNTAX ERROR at line 4: Invalid watch definition - WATCH: 200-G
%test%error-7.hex

echo EXPECT/ACTUAL error-8.hex
echo ERROR: The value '6y' specified for the length in the watch ' 200-a-6y' is not a valid hexadecimal number!
echo SYNTAX ERROR at line 4: Invalid watch definition - WATCH: 200-a-6y
%test%error-8.hex

echo EXPECT/ACTUAL pass-1.hex
echo CPU .... : PC $0000      A $08      X $00      Y $00      SP $fa       S 00100100
echo WATCH 01 : $0200: $01 $05 $08 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00
echo WATCH 02 : $0600: $a9 $01 $8d $00 $02 $a9 $05 $8d $01 $02 $a9 $08 $8d $02 $02 $00
%test%pass-1.hex

echo EXPECT/ACTUAL pass-2.hex
echo CPU .... : PC $0000      A $08      X $00      Y $00      SP $fa       S 00100100
echo WATCH 01 : $0200: $01 $05 $08 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00
echo WATCH 02 : $0600: $a9 $01 $8d $00 $02 $a9 $05 $8d $01 $02 $a9 $08 $8d $02 $02 $00
echo WATCH 03 : $0600: This is a string that will be null terminated.
%test%pass-2.hex

echo EXPECT/ACTUAL pass-3.hex
echo CPU .... : PC $040f      A $08      X $00      Y $00      SP $fd       S 00100000
echo WATCH 01 : $0200: $01 $05 $08 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00
echo WATCH 02 : $0400: $a9 $01 $8d $00 $02 $a9 $05 $8d $01 $02 $a9 $08 $8d $02 $02 $00
%test%pass-3.hex

echo EXPECT/ACTUAL pass-4.hex
echo CPU .... : PC $040f      A $08      X $00      Y $00      SP $fd       S 00100000
echo WATCH 01 : $0200: $01 $05 $08 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00
echo WATCH 02 : $0400: $a9 $01 $8d $00 $02 $a9 $05 $8d $01 $02 $a9 $08 $8d $02 $02 $00
%test%pass-4.hex

echo EXPECT/ACTUAL pass-5.hex
echo CPU .... : PC $070c      A $0c      X $00      Y $00      SP $fd       S 00100000
echo WATCH 01 : $0200: $0a $0b $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00
echo WATCH 02 : $0700: $a9 $0a $8d $00 $02 $a9 $0b $8d $01 $02 $a9 $0c $8d $02 $02 $00
echo WATCH 03 : $0800: Hello world!
%test%pass-5.hex

echo EXPECT/ACTUAL pass-6.hex
echo Hello World!
%test%pass-6.hex

echo EXPECT/ACTUAL pass-7.hex
echo +----------------------------------------------------------------+
echo ^|Daniel                                                          ^|
echo ^| Hello world!                                                   ^|
echo ^|  Hello all!                                                    ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo ^|                                                                ^|
echo +----------------------------------------------------------------+
%test%pass-7.hex

echo EXPECT/ACTUAL pass-8.hex
echo Exit
echo 0
%test%pass-8.hex
echo %ERRORLEVEL%

echo EXPECT/ACTUAL pass-9.hex
echo Exit
echo 1
%test%pass-9.hex
echo %ERRORLEVEL%

echo EXPECT/ACTUAL pass-10.hex
echo Hi there!
%test%pass-10.hex

echo EXPECT/ACTUAL pass-11.hex
echo WATCH 01 : $0200: 16 x random numbers
%test%pass-11.hex

echo EXPECT/ACTUAL pass-12.hex
echo Hello World!
echo This should also get output!
%test%pass-12.hex

echo EXPECT/ACTUAL pass-13.hex
echo Using a relative jump to a label!
%test%pass-13.hex

echo "EXPECT/ACTUAL pass-14.hex"
echo 254
echo -2
echo 65244
echo -292
echo 16702650
echo -74566
echo 4275878552
echo -19088744
%test%pass-14.hex

echo "EXPECT/ACTUAL pass-15.hex"
echo 255
echo 48762187
echo -134534
echo -1
%test%pass-15.hex
