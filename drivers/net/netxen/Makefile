# Copyright (C) 2003 - 2006 NetXen, Inc.
# All rights reserved.
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#                            
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#                                   
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA  02111-1307, USA.
# 
# The full GNU General Public License is included in this distribution
# in the file called LICENSE.
# 
# Contact Information:
#    info@netxen.com
# NetXen,
# 3965 Freedom Circle, Fourth floor,
# Santa Clara, CA 95054
#
# Makefile for the NetXen NIC Driver
#


obj-$(CONFIG_NETXEN_NIC) := netxen_nic.o

netxen_nic-y := netxen_nic_hw.o netxen_nic_main.o netxen_nic_init.o \
	netxen_nic_isr.o netxen_nic_ethtool.o netxen_nic_niu.o
