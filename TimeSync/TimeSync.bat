@echo off

:user_input
  echo Please input the comport number:
  set /p com=COM (e.g. 1-100)?

  echo %com%| findstr /r "^[1-9][0-9]*$">nul

  rem input is a number goto sync the clock
  if %errorlevel% equ 0 (            
      goto sync
  )

  if not %errorlevel% equ 0 (
    echo Not a Valid Comport
    goto user_input
  )

:sync

  mode COM%com% BAUD=9600 PARITY=n DATA=8

  for /F "usebackq tokens=1,2 delims==" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set ldt=%%j
  set hours=%ldt:~8,2%
  set minutes=%ldt:~10,2%
  set /A seconds=%ldt:~12,2%+2
  set miliSeconds=%ldt:~15,3%
  :loop
  if %TIME% LSS %hours%:%minutes%:%seconds%.00 goto loop
  for /F "usebackq tokens=1,2 delims==" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set ldt=%%j
  echo S%ldt:~12,2%, >COM%com%
  echo D%ldt:~10,2%, >COM%com%
  echo H%ldt:~8,2%, >COM%com%

  echo S%ldt:~12,2%, 
  echo D%ldt:~10,2%,
  echo H%ldt:~8,2%,
  set /A seconds=%ldt:~12,2%+4
  :loop2
  if %TIME% LSS %hours%:%minutes%:%seconds%.00 goto loop2

  echo T%ldt:~6,2%, >COM%com%
  echo M%ldt:~4,2%, >COM%com%
  echo J%ldt:~0,4%, >COM%com%

  echo T%ldt:~6,2%,
  echo M%ldt:~4,2%,
  echo J%ldt:~0,4%,


:leave