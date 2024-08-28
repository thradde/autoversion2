# Autoversion: a tool for versioning large projects

## What it does
Basically, autoversion is an automated search & replace tool. By using a control file, you specify the affected files, what to search for in each file and with what to replace it.

If you have large projects where you need to change version numbers in many places, autoversion comes in handy.

## Example control file:
@Version		"v4.10"  
@LongVersion	"v4.10.0.0"  

&"main.cpp"		"v4.00"			@Version  
&"resource.rc"	"v4.00.5.0"		@LongVersion  

The interesting part is that autoversion updates the control file itself, after all replacements have been performed successfully. After the replacement, the control file will look like this:

@Version		"v4.10"  
@LongVersion	"v4.10.0.0"  

&"main.cpp"		"v4.10"			@Version  
&"resource.rc"	"v4.10.0.0"		@LongVersion  

This way, if you make a new release, you only need to change the @-constant definitions, but not the search/replace instructions.

**For further details and usage, see the file "Auto Version.doc".**

## Supported Platforms
Currently, the code is only running on Windows, but making it cross-platform is simple, just make the path separator "\\" compile platform dependent into "\\" or "/".
