Set cloner = CreateObject("WScript.Shell")

cloner.Run "cmd"
WScript.Sleep 500

cloner.SendKeys "{Enter}"
cloner.SendKeys "telnet 192.168.5.1"
cloner.SendKeys "{Enter}"
WScript.Sleep 500

cloner.SendKeys "root"
cloner.SendKeys "{Enter}"
WScript.Sleep 500

cloner.SendKeys "root"
cloner.SendKeys "{Enter}"
WScript.Sleep 500

cloner.SendKeys "nvram kset 0:wb_txbuf_offset_5gbw160=51"
cloner.SendKeys "{Enter}"
WScript.Sleep 300


cloner.SendKeys "nvram kcommit "
cloner.SendKeys "{Enter}"
WScript.Sleep 300


cloner.SendKeys "reboot"
cloner.SendKeys "{Enter}"













 
