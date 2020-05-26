import time
import requests
import sys
import subprocess

catch=[]
frequency=20 #sec

def resentcatch():
    while(len(catch)>0):
        url=catch[-1]
        try:
            x = requests.post(url,timeout=5)
            print(x.text)
            catch.pop()
        except:
            print("Can't connect during tring to resent : ",len(catch))
            return False

def SentToServer(serial,t,co2):
    url = f"https://co2api.herokuapp.com/summit?serial={serial}&t={t}&co2={co2}"
    try:
        x = requests.post(url,timeout=5)
        print(x.text)
    except:
        print("Can't connect to sever : ",len(catch))
        catch.append(url)

def getvalue():
    try:
        p = subprocess.check_output(["dusbrelays/usbtenkiget","-i","0"]).decode(sys.stdout.encoding)
        return p.split(",")[0]
    except subprocess.CalledProcessError:
        print("usbtenkiget error")
        return -1



nexttime = time.time()
serial = open("serial.txt", "r").readline()
while(True):
    if(time.time()>nexttime):
        print("Get value : ",time.time())
        SentToServer(serial,time.time(),getvalue())
        nexttime=nexttime+frequency
        print("Next time: ",nexttime)
        print("_______________________")
    if(len(catch)>0):
        resentcatch()