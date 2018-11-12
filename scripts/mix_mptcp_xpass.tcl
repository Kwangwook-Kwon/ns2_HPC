set ns [new Simulator]

# Configurations
set linkBW 10Gb
set ALPHA 1.0
set w_init 0.5
set linkBW 10Gb
set linkLatency 100us
set creditQueueCapacity [expr 84*10] ;# Bytes
set dataQueueCapacity [expr 1538*100] ;# Bytes
set creditRate 64734895 ;# bytes/sec

Agent/MPTCP set K 1


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
#Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
#Queue/XPassDropTail set data_limit_ $dataQueueCapacity
#Queue/XPassDropTail set token_refresh_rate_ $creditRate


set r1 [$ns node]
set r2 [$ns node]
$ns trace-all $nt

$ns simplex-link $node0_0 $r1      $linkBW $linkLatency DropTail
$ns simplex-link $r1	  $node0_0   $linkBW $linkLatency XPassDropTail
$ns simplex-link $node1_0 $r1      $linkBW $linkLatency DropTail
$ns simplex-link $r1	  $node1_0   $linkBW $linkLatency XPassDropTail


$ns simplex-link $node0_1 $r2      $linkBW $linkLatency DropTail
$ns simplex-link $r2      $node0_1 $linkBW $linkLatency XPassDropTail
$ns simplex-link $node1_1 $r2      $linkBW $linkLatency DropTail
$ns simplex-link $r2      $node1_1 $linkBW $linkLatency XPassDropTail



Agent/XPass set max_credit_rate_ $creditRate
Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set w_ $w_init
Agent/XPass set alpha_ $ALPHA



#
#Sender
#

puts "Creating Sender Agents..."
set mptcp [new Agent/MPTCP]
set mptcp2 [new Agent/MPTCP]
set agent0 [new Agent/XPass]
set agent1 [new Agent/XPass]
set agent101 [new Agent/XPass]
set agent102 [new Agent/XPass]
$ns attach-agent $node0_0 $agent0
$ns attach-agent $node0_1 $agent1
$ns attach-agent $node0_0 $agent101
$ns attach-agent $node0_1 $agent102
$agent0 set max_credit_rate_ $creditRate
$agent1 set max_credit_rate_ $creditRate
$agent0 set alpha_  $ALPHA
$agent1 set alpha_  $ALPHA
$mptcp attach-xpass $agent0
$mptcp attach-xpass $agent1
$mptcp2 attach-xpass $agent101
$mptcp2 attach-xpass $agent102
$ns multihome-attach-agent $node0 $mptcp
$ns multihome-attach-agent $node0 $mptcp2


#
#Create mptcp  reciever
#
puts "Creating Receiver Agents..."
set mptcpsink [new Agent/MPTCP]
set mptcpsink2 [new Agent/MPTCP]

set agent2 [new Agent/XPass]
set agent3 [new Agent/XPass]
set agent102 [new Agent/XPass]
set agent103 [new Agent/XPass]
$ns attach-agent $node1_0 $agent2
$ns attach-agent $node1_1 $agent3
$ns attach-agent $node1_0 $agent102
$ns attach-agent $node1_1 $agent103
$agent2 set max_credit_rate_ $creditRate
$agent3 set max_credit_rate_ $creditRate
$agent2 set alpha_  $ALPHA
$agent3 set alpha_  $ALPHA

$mptcpsink attach-xpass $agent2
$mptcpsink attach-xpass $agent3

$mptcpsink2 attach-xpass $agent102
$mptcpsink2 attach-xpass $agent103

$ns multihome-attach-agent $node1 $mptcpsink
$ns multihome-attach-agent $node1 $mptcpsink2

$ns multihome-connect $mptcp $mptcpsink
$ns multihome-connect $mptcp2 $mptcpsink2

$mptcpsink listen




puts "Simulation started."
$ns at 0.0 "$mptcp send-msg 2000000000"
$ns at 0.0 "$mptcp2 send-msg 2000000000"

$ns at 10.0 "finish"
$ns run

