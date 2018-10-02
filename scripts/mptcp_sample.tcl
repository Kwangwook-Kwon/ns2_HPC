set ns [new Simulator]

# Configurations
set linkBW 10Gb
set linkLatency 10us
set creditQueueCapacity [expr 84*10] ;# Bytes
set dataQueueCapacity [expr 1538*100] ;# Bytes
set creditRate 64734895 ;# bytes/sec

# Output file
file mkdir "outputs"
set nt [open outputs/trace.out w]
set fct_out [open outputs/fct.out w]
puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
close $fct_out

proc finish {} {
  global ns nt
  $ns flush-trace
  close $nt
  puts "Simulation terminated successfully."
  exit 0
}
$ns trace-all $nt
puts "Creating Nodes..."
set node0 [$ns node]
set node0_0 [$ns node]
set node0_1 [$ns node]
set node1 [$ns node]
set node1_0 [$ns node]
set node1_1 [$ns node]

#
#MUltipath-Sender
#
$node0 color red
$node0_0 color red
$node0_1 color red
$ns multihome-add-interface $node0 $node0_0
$ns multihome-add-interface $node0 $node0_1

#
#Multipath-Reciever
#
$node1 color blue
$node1_0 color blue
$node1_1 color blue
$ns multihome-add-interface $node1 $node1_0
$ns multihome-add-interface $node1 $node1_1


#
# Interface nodes
#

set r1 [$ns node]
set r2 [$ns node]
set r3 [$ns node]
set r4 [$ns node]

$ns duplex-link $node0_0 $r1 10Mb 5ms DropTail
$ns duplex-link $r1      $r3 1Mb  5ms DropTail
$ns queue-limit $r1      $r3 30
$ns duplex-link $node1_0 $r3 10Mb 5ms DropTail

$ns duplex-link $node0_1 $r2 10Mb 5ms DropTail
$ns duplex-link $r2      $r4 1Mb  5ms DropTail
$ns queue-limit $r2      $r4 30
$ns duplex-link $node1_1 $r2 10Mb 5ms DropTail


#
#Create mptcp  sender
#

set tcp0 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp0 set window_ 100
$ns attach-agent $node0_0 $tcp0

set tcp1 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp1 set window_ 100
$ns attach-agent $node0_1 $tcp1

set mptcp [new Agent/MPTCP]
$mptcp attach-tcp $tcp0
$mptcp attach-tcp $tcp1
$ns multihome-attach-agent $node0 $mptcp

set ftp [new Application/FTP]
$ftp attach-agent $mptcp



#
#Create mptcp  reciever
#
set mptcpsink [new Agent/MPTCP]
set sink0 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $node1_0 $sink0

set sink1 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $node1_1 $sink1

$mptcpsink attach-tcp $sink0
$mptcpsink attach-tcp $sink1

$ns multihome-attach-agent $node1 $mptcpsink
$ns multihome-connect $mptcp $mptcpsink
$mptcpsink listen

puts "Simulation started."
$ns at 0.0 "$ftp start"
$ns at 3.0 "finish"
$ns run

