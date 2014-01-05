#!/usr/bin/env python
"""
Creates files that look like:

o<g70> sub
G0 Z0
G0 Y80
G0 Z-5
o<g70> endsub
M2

G70-72 move to three circle positions.
G73 moves to the center.
"""
from math import sin, cos, pi
import os

r = 80
z_travel = 7
z1 = 0
z2 = z1 - z_travel

delta_dir = "configs/ARM/BeagleBone/BeBoPr/delta"
if not os.path.exists(delta_dir):
    os.path.makedirs(delta_dir)

def g0(x = None, y = None, z = None):
    x_s = " X%s" % x if x is not None else ""
    y_s = " Y%s" % y if y is not None else ""
    z_s = " Z%s" % z if z is not None else ""
    return "G0%s%s%s\n" % (x_s, y_s, z_s) 

def write_ngc(g_code_index, contents, g_code_base=70):
    g_code = g_code_base + g_code_index
    filename = "g%s.ngc" % g_code
    print "Making %s" % filename
    s = "o<g%i> sub\n" % g_code
    s += "%s" % contents
    s += "o<g%i> endsub\n" % g_code
    s += "M2\n"
    f = open(os.path.join(delta_dir, filename), 'w')
    f.write(s)

# Add each point around the circle
for i, theta in enumerate(range(0, -360, -120)):
    print "theta = %s" % theta
    s = g0(z = z1)
    theta_r = (2 * pi / 360.0) * theta
    x = sin(theta_r) * r
    y = cos(theta_r) * r
    s += g0(x = x, y = y)
    s += g0(z = z2)  
    write_ngc(i, s)

# Add center
s = g0(z = z1)
s += g0(x = 0.0, y = 0.0)
s += g0(z = z2)
write_ngc(9, s)
