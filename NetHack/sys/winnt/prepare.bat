:: Requires GnuWin32 iconv, mv
SETLOCAL ENABLEEXTENSIONS

REM switch to the top directory
CD /d "%0\..\..\.." 1>NUL 2>&1

REM transform coding
FOR %%X IN (src\*.*, include\artilist.h, dat\*.*, win\win32\*.c) DO (
	iconv.exe -c -f utf-8 -t gb18030 %%X 1> %%X.gb
	mv.exe %%X.gb %%X
)

ENDLOCAL
