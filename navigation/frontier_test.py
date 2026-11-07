# navigation/frontier_test.py
# 作用：导航、定位、巡检执行
# 模块：导航、定位、巡检执行
# 文档：docs/代码结构说明.md

import math
import unittest
from frontier_explorer import select_frontiers


def room(open_edge=True):
    """「room」的实现。

    参数：open_edge。
    返回：计算结果。
    """
    w=h=40;data=[-1]*(w*h)
    for y in range(5,35):
        for x in range(5,35):data[y*w+x]=0
    for y in range(4,36):
        data[y*w+4]=data[y*w+35]=100
    for x in range(4,36):data[4*w+x]=100;data[35*w+x]=-1 if open_edge else 100
    return dict(width=w,height=h,resolution=.1,origin=(0,0,0),data=data)


class Frontiers(unittest.TestCase):
    """Frontiers 类。"""
    def test_known_free_standoff_target(self):
        """「test_known_free_standoff_target」的实现。"""
        grid=room();r=select_frontiers(grid,dict(x=2,y=2),.4)
        self.assertTrue(r['candidates']);self.assertFalse(r['complete'])
        target=r['candidates'][0]
        self.assertEqual(grid['data'][int(target['y']/.1)*40+int(target['x']/.1)],0)
        self.assertLess(target['y'],3.1)

    def test_closed_room_complete(self):
        """「test_closed_room_complete」的实现。"""
        self.assertTrue(select_frontiers(room(False),dict(x=2,y=2),.4)['complete'])

    def test_blocked_start_rejected(self):
        """「test_blocked_start_rejected」的实现。"""
        with self.assertRaises(ValueError):select_frontiers(room(),dict(x=.55,y=.55),.4)

    def test_excluded_goals_not_repeated(self):
        """「test_excluded_goals_not_repeated」的实现。"""
        r=select_frontiers(room(),dict(x=2,y=2),.4,[dict(x=x/10,y=y/10) for x in range(0,40,3) for y in range(0,40,3)])
        self.assertFalse(r['candidates']);self.assertFalse(r['complete'])

    def test_rotated_map_coordinates(self):
        """「test_rotated_map_coordinates」的实现。"""
        grid=room();grid['origin']=(10,20,math.pi/2)
        r=select_frontiers(grid,dict(x=8,y=22),.4)
        self.assertTrue(r['candidates']);self.assertLess(r['candidates'][0]['x'],8)

    def test_unknown_barrier_not_crossed(self):
        """「test_unknown_barrier_not_crossed」的实现。"""
        grid=room()
        for x in range(5,35):grid['data'][20*40+x]=-1
        r=select_frontiers(grid,dict(x=2,y=1.2),.3)
        self.assertTrue(all(t['y']<2 for t in r['candidates']))


if __name__=='__main__':unittest.main()
