set ns [new Simulator]

# Configurations
set N 6
set S 2
set ALPHA [expr 1./32.]
set w_init [expr 1./32.]
set creditQueueCapacity [expr 84*2]  ;# bytes
set dataQueueCapacity [expr 1538*100] ;# bytes
set hostQueueCapacity [expr 1538*100] ;# bytes
set maxCrditBurst [expr 84*2] ;# bytes
set creditRate 64734895 ;# bytes / sec
set interFlowDelay 0 ;# secs

#
# Toplogy configurations
#
set linkRate 10 ;# Gb
set hostDelay 0.000001 ;# secs
set linkDelayRouterHost   0.000004 ;#secs
set linkDelayRouterRouter 0.000004 ;#secs
set dataBufferHost [expr 1000*1538] ;# bytes / port
set dataBufferRouterHost [expr 250*1538] ;#bytes / port
set dataBufferRouterRouter [expr 250*1538] ;# bytes / port

# Output file
file mkdir "outputs"
set nt [open outputs/trace.out w]
set fct_out [open outputs/mpath_fct.out w]
puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
close $fct_out

proc finish {} {
  global ns nt
  $ns flush-trace
  close $nt
  puts "Simulation terminated successfully."
  exit 0
}

#$ns trace-all $nt

puts "Creating Nodes..."

set leftTerm [$ns node]
set rightTerm [$ns node]

for {set i 0} {$i < $S} {incr i} {
set leftTerm_sub($i)  [$ns node]
set rightTerm_sub($i) [$ns node]
$ns multihome-add-interface $leftTerm $leftTerm_sub($i)
$ns multihome-add-interface $rightTerm $rightTerm_sub($i)
}

for {set i 0} {$i < $N} {incr i} {
  set host($i) [$ns node]
	set router($i) [$ns node]
  for {set j 0} {$j < $S} {incr j} {
  set host_sub($i,$j) [$ns node]
  $ns multihome-add-interface $host($i) $host_sub($i,$j)
  }
}

puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set qlim_ [expr $hostQueueCapacity/1538]

Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

for {set i 0} {$i < $N} {incr i} {
	if {$i > 0} {
		$ns duplex-link $router([expr $i - 1]) $router($i) [set linkRate]Gb $linkDelayRouterRouter XPassDropTail
		set link_rl [$ns link $router([expr $i - 1]) $router($i)]
		set queue_rl [$link_rl queue]
		$queue_rl set data_limit_ $dataBufferRouterRouter
		$ns trace-queue $router([expr $i - 1]) $router($i) $nt
	}	
	for {set j 0} {$j < $S} {incr j} {
	  $ns duplex-link $host_sub($i,$j) $router($i) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
	  set link_ll [$ns link $host_sub($i,$j) $router($i)]
	  set queue_ll [$link_ll queue]
	  $queue_ll set data_limit_ $dataBufferRouterHost
  }
}
for {set i 0} {$i < $S} {incr i} {
  $ns duplex-link $leftTerm_sub($i) $router(0) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
  set link_lt [$ns link $leftTerm_sub($i)  $router(0)]
  set queue_lt [$link_lt queue]
  $queue_lt set data_limit_ $dataBufferRouterHost

  $ns duplex-link $rightTerm_sub($i) $router([expr $N - 1]) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
  set link_rt [$ns link $rightTerm_sub($i) $router([expr $N - 1])]
  set queue_rt [$link_rt queue]
  $queue_rt set data_limit_ $dataBufferRouterHost
}

puts "Creating Agents..."
Agent/XPass set max_credit_rate_ $creditRate
#Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set alpha_ $ALPHA
Agent/XPass set w_ $w_init

for {set i 0} {$i < [expr $N - 1]} {incr i} {
  set mp_sender($i) [new Agent/MPTCP]
  set mp_receiver($i) [new Agent/MPTCP]
  for {set j 0} {$j < $S} {incr j} {
    set sender_sub($i,$j)   [new Agent/XPass]
    set receiver_sub($i,$j) [new Agent/XPass]
    $ns attach-agent $host_sub($i,$j) $sender_sub($i,$j)
    $ns attach-agent $host_sub([expr $i+ 1],$j) $receiver_sub($i,$j)
    $mp_sender($i) attach-xpass $sender_sub($i,$j)
    $mp_receiver($i) attach-xpass $receiver_sub($i,$j)
    #$ns connect $sender($i) $receiver($i)
  }
  $ns multihome-attach-agent $host($i) $mp_sender($i)
  $ns multihome-attach-agent $host([expr $i+ 1]) $mp_receiver($i)
  $ns multihome-connect $mp_sender($i) $mp_receiver($i)
  $mp_sender($i) set fid_ $i
  $mp_receiver($i) set fid_ $i
}

set longest [expr $N - 1]
set mp_sender($longest) [new Agent/MPTCP]
set mp_receiver($longest) [new Agent/MPTCP]
for {set j 0} {$j < $S} {incr j} {
  set sender_sub($longest,$j) [new Agent/XPass]
  set receiver_sub($longest,$j) [new Agent/XPass]
  $ns attach-agent $leftTerm_sub($j) $sender_sub($longest,$j)
  $ns attach-agent $rightTerm_sub($j) $receiver_sub($longest,$j)
  $mp_sender($longest) attach-xpass $sender_sub($longest,$j)
  $mp_receiver($longest) attach-xpass $receiver_sub($longest,$j)
}
$ns multihome-attach-agent $leftTerm $mp_sender($longest)
$ns multihome-attach-agent $rightTerm $mp_receiver($longest)
$ns multihome-connect $mp_sender($longest) $mp_receiver($longest)
#$ns connect $sender($longest) $receiver($longest)
$mp_sender($longest) set fid_ $longest
$mp_receiver($longest) set fid_ $longest


puts "Simulation started."
set nextTime 0.0
for {set i 0} {$i < $N} {incr i} {
#  $ns at $nextTime "$mp_sender($i) send-msg 100000000"
$ns at $nextTime "$mp_sender($i) send-msg 10000"
  set nextTime [expr $nextTime + $interFlowDelay]
}

$ns at 30.0 "finish"
$ns run
