set ns [new Simulator]

# Configurations
set ALPHA 0.5
set w_init 0.5
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
$ns multihome-add-interface $node0 $node0_0
$ns multihome-add-interface $node0 $node0_1

#
#Multipath-Reciever
#
$ns multihome-add-interface $node1 $node1_0
$ns multihome-add-interface $node1 $node1_1


$ns trace-all $nt

puts "Creating Links..."
Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate


set r1 [$ns node]
set r2 [$ns node]

$ns simplex-link $node0_0 $r1      10Mb 5ms DropTail
$ns simplex-link $r1	  $node0_0  10Mb 5ms XPassDropTail
$ns simplex-link $node1_0 $r1      10Mb 5ms DropTail
$ns simplex-link $r1	  $node1_0 10Mb 5ms XPassDropTail

$ns simplex-link $node0_1 $r2      10Mb 5ms DropTail
$ns simplex-link $r2      $node0_1   10Mb 5ms XPassDropTail
$ns simplex-link $node1_1 $r2      10Mb 5ms DropTail
$ns simplex-link $r2      $node1_1 10Mb 5ms XPassDropTail


#Agent/XPass set max_credit_rate_ $creditRate
Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set w_ $w_init



#
#Sender
#

puts "Creating Sender Agents..."
set mptcp [new Agent/MPTCP]
set agent0 [new Agent/XPass]
set agent1 [new Agent/XPass]
$ns attach-agent $node0_0 $agent0
$ns attach-agent $node0_1 $agent1
$mptcp attach-xpass $agent0
$mptcp attach-xpass $agent1
$ns multihome-attach-agent $node0 $mptcp


#
#Create mptcp  reciever
#
puts "Creating Receiver Agents..."
set mptcpsink [new Agent/MPTCP]

set agent2 [new Agent/XPass]
$ns attach-agent $node1_0 $agent2
set agent3 [new Agent/XPass]
$ns attach-agent $node1_1 $agent3

$mptcpsink attach-xpass $agent2
$mptcpsink attach-xpass $agent3

$ns multihome-attach-agent $node1 $mptcpsink
$ns multihome-connect $mptcp $mptcpsink
#$mptcpsink listen




puts "Simulation started."
$ns at 0.0 "$mptcp send-msg 1000000"
$ns at 3.0 "finish"
$ns run

