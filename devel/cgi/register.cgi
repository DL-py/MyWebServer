#!/usr/bin/python3
#coding:utf-8
import sys,os
import urllib
from urllib import parse

content_length = int(sys.argv[1])
# print("content_length: ", content_length)

url = sys.stdin.read(content_length)
# print("url: ", url)

params_lst = url.split("&")

params_dict = dict()
for param in params_lst:
    # print("param: ", param)
    key, val = param.split("=")
    key_decoded = parse.unquote_plus(key)
    val_decoded = parse.unquote_plus(val)
    params_dict[key_decoded] = val_decoded

# construct response:
print('<html>')
print('<head>' )
print('<title>Login</title>')
print('<meta charset="utf-8">')
print('</head>' )
print('<body>')
print('<h1 ">该用户已存在</h1>')
print('<br><br>')
print('<a href="welcome.html">')
print('<button style="height:50;wIDth:100;", align="center">返回主页</button></center>')
print('</a>')
print ('</body>')
print ('</html>')

