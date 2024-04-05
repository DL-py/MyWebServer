#!/usr/bin/python3
#coding:utf-8
import sys,os
import urllib
length = os.getenv('CONTENT_LENGTH')
is_correct = os.getenv("IsCorrect")
if int(is_correct) == 1:
    postdata = sys.stdin.read(int(length))
    print ('<html>') 
    print ('<head>' )
    print ('<title>Login</title>') 
    print('<meta charset="utf-8">')
    print ('</head>' )
    print ('<body>') 
    print ('<h1>您的个人信息为：</h1>') 
    print ('<ul >')
    for data in postdata.split('&'):
    	print ('<h3>'+data+'</h3>') 
    print ('</ul>')
    print('<a href="welcome.html">')
    print('<button style="height:50;wIDth:100;", align="center">返回主页</button></center>')
    print('</a>')
    print ('</body>')
    print ('</html>')
    print ('\n')
else:
    print ('<html>') 
    print ('<head>' )
    print ('<title>Login</title>') 
    print('<meta charset="utf-8">')
    print ('</head>' )
    print ('<body>') 
    print ('<h1 ">用户不存在或密码输入有误</h1>') 
    print('<br><br>')
    print('<a href="welcome.html">')
    print('<button style="height:50;wIDth:100;", align="center">返回登录</button></center>')
    print('</a>')
    print ('</body>')
    print ('</html>')




