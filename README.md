# Server Timebomb

This plugin is a timebomb. Yup, you read it right. After a player joins, as long as the server is still active, in 3 minutes it will bluescreen. This will only work on Windows, though.

# Why did I make this?

Well the day before I was bored and too lazy to shut down my PC, so I wrote a C++ app that made itself a critical process and then terminated itself, thus bluescreening my PC. Some friend called ugng then came to me with a few more undocumented WinAPI functions, such as NtRaiseHardError. Alongside another that I found (RtlAdjustPrivilege), I managed to compile this.

Basically, boredom is the reason. Thanks though, ugng.
