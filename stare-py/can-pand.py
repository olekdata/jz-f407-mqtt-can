#!/usr/bin/python
# -*- coding: utf-8 -*-
# czyta komunikaty z magistrali CAN
# zapisuje do tablicy biblioteki pandas
# wyœwietla odpowiednio posortowane
#

import os
import can
import pandas as pd
import numpy as np


#os.system('sudo ip link set can0 type can bitrate 100000')
#os.system('sudo ifconfig can0 up')


def ScrCursor(y, x):
    print("\033[%d;%dH" % (y, x))
    
can0 = can.interface.Bus(channel = 'can0', bustype = 'socketcan_ctypes')# socketcan_native
df = pd.DataFrame(columns = ['count',  'ext_id', 'id', 'dlc', 'data'])

#msg = can.Message(arbitration_id=0x123, data=[0, 1, 2, 3, 4, 5, 6, 7], extended_id=False)

os.system('cls||clear')




i = 0

while 1:
    msg = can0.recv(10.0)
    if not msg is None:
        id = msg.arbitration_id
        if not(id in df.index):
            df.loc[id]=[0, -1, '', 0,[]]
        df.loc[id]['id']= hex(id).rstrip("L")
        df.loc[id]['dlc']= msg.dlc
        df.loc[id]['count'] += 1
        df.loc[id]['data'] = msg.data
        df.loc[id]['ext_id'] = msg.is_extended_id   


        

        
#        dl = []
#        for d in msg.data
#            dl.append(d)
        df.loc[id]['data'] = msg.data
        
        
    ScrCursor(0,0)
    print(df.sort_values(by='count', ascending=False)[:45])
#    for r in df.items():
#        print(r)
    i += 1
    if i % 100 == 0:
#        df.to_csv('can-pand.cvs', index=True);
        print("Zapis")
    else:
        print(str(i)+"       ")
        
   

#os.system('sudo ifconfig can0 down')

'''
#ad_ds = pd.interval_range(start=1, end=2, periods=1)
#ad_df = pd.DataFrame([[1, 0] , [9, 0]], columns = ['dlc', 'count'])
#ad_df = pd.Series([])
#print(ad_df)

i = 4
if not(i in ad_df.index):
    ad_df.loc[i]=[0,0]

ad_df.loc[i]['count'] += 1

ad_df.sort_values(by='dlc')

print(ad_df.tail())
'''