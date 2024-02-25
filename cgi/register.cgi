#!/usr/bin/python3
#coding:utf-8
import sys,os
import urllib
length = os.getenv('CONTENT_LENGTH')
print("length:", length)
is_repeat = os.getenv('Is_Repeat')

if int(is_repeat) > 0:
    print ('<html>') 
    print ('<head>' )
    print ('<title>Login</title>') 
    print('<meta charset="utf-8">')
    print ('</head>' )
    print ('<body>') 
    print ('<h1 ">该用户已存在</h1>') 
    print('<br><br>')
    print('<a href="welcome.html">')
    print('<button style="height:50;wIDth:100;", align="center">返回主页</button></center>')
    print('</a>')
    print ('</body>')
    print ('</html>')
elif length:
    postdata = sys.stdin.read(int(length))
    print ('<html>') 
    print ('<head>' )
    print ('<title>POST</title>') 
    print('<meta charset="utf-8">')
    print ('</head>' )
    print ('<body>') 
    print ('<h1> 注册成功 </h1>')
    print('<a href="welcome.html">')
    print('<button style="height:50;wIDth:100;">返回主页</button>')
    print('</a>')
    print ('</body>')
    print ('</html>')
    print ('\n')
else:
    print ("Content-type:text/html\n")
    print ('no found')


