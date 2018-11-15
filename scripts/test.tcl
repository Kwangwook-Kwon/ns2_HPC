set ns [new Simulator]

#
# Flow configurations
#cd ns
set numFlow 100000
set workload "cachefollower" ;# cachefollower, mining, search, webserver
set linkLoad 0.6 ;# ranges from 0.0 to 1.0

#
# Toplogy configurations
#
set linkRate 10 ;# Gb
set hostDelay 0.000001 ;# secs
set linkDelayHostTor 0.000004 ;# secs
set linkDelayTorAggr 0.000004 ;# secs
set linkDelayAggrCore 0.000004 ;# secs
set dataBufferHost [expr 1000*1538] ;# bytes / port
set dataBufferFromTorToAggr [expr 250*1538] ;# bytes / port
set dataBufferFromAggrToCore [expr 250*1538] ;# bytes / port
set dataBufferFromCoreToAggr [expr 250*1538] ;# bytes / port
set dataBufferFromAggrToTor [expr 250*1538] ;# bytes / port
set dataBufferFromTorToHost [expr 250*1538] ;# bytes / port

set numCore 4 ;# number of core switches
set numAggr [expr $numCore*4] ;# number of aggregator switches
set numTor [expr $numCore*4] ;# number of ToR switches
set numNode [expr $numTor*5 ] ;# number of nodes
set N $numCore;
set K [expr $N/2];


#
# XPass configurations
#
set alpha 0.5
set w_init 0.0625
set creditBuffer [expr 84*4]
set maxCreditBurst [expr 84*2]
#set controlBuffer [expr 84*100]
set minJitter -0.1
set maxJitter 0.1
set minEthernetSize 84
set maxEthernetSize 1538
set minCreditSize 76
set maxCreditSize 92
set xpassHdrSize 78
set maxPayload [expr $maxEthernetSize-$xpassHdrSize]
set avgCreditSize [expr ($minCreditSize+$maxCreditSize)/2.0]
set creditBW [expr $linkRate*125000000*$avgCreditSize/($avgCreditSize+$maxEthernetSize)]
set creditBW [expr int($creditBW)]

#
# Simulation setup
#
set simStartTime 0.1
set simEndTime 60

# Output file
file mkdir "outputs"
set mpath_fct [open "outputs/mp_fct.out" w]
set nt [open "outputs/trace.out" w]
set fct_out [open "outputs/fct.out" w]
set wst_out [open "outputs/mp_waste.out" w]
puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
puts $mpath_fct "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
puts $wst_out "Flow ID,Flow Size (bytes),Wasted Credit"
close $fct_out
close $wst_out
close $mpath_fct

set flowfile [open flowfile.tr w]

proc finish {} {
  global ns nt flowfile
  $ns flush-trace
  close $nt
  close $flowfile
  puts "Simulation terminated successfully."
  exit 0
}
#$ns trace-all $nt

# Basic parameter settings
Agent/MPTCP set K $K

Agent/XPass set min_credit_size_ $minCreditSize
Agent/XPass set max_credit_size_ $maxCreditSize
Agent/XPass set min_ethernet_size_ $minEthernetSize
Agent/XPass set max_ethernet_size_ $maxEthernetSize
Agent/XPass set max_credit_rate_ $creditBW
Agent/XPass set alpha_ $alpha
Agent/XPass set target_loss_scaling_ 0.125
Agent/XPass set w_init_ $w_init
Agent/XPass set min_w_ 0.01
Agent/XPass set retransmit_timeout_ 0.0001
Agent/XPass set min_jitter_ $minJitter
Agent/XPass set max_jitter_ $maxJitter

Queue/XPassDropTail set credit_limit_ $creditBuffer
#Queue/XPassDropTail set control_q_limit_ $controlBuffer
Queue/XPassDropTail set max_tokens_ $maxCreditBurst
Queue/XPassDropTail set token_refresh_rate_ $creditBW

DelayLink set avoidReordering_ true
$ns rtproto DV
Agent/rtProto/DV set advertInterval 10
Node set multiPath_ 1
Classifier/MultiPath set ecmp_ 1
Classifier/MultiPath set symmetric_ 0
Classifier/MultiPath set perflow_ 0
Classifier/MultiPath set nodetype_ 0
Classifier/MultiPath set numCore_ $numCore

# Workloads setting
if {[string compare $workload "mining"] == 0} {
  set workloadPath "workloads/workload_mining.tcl"
  set avgFlowSize 7410212
} elseif {[string compare $workload "search"] == 0} {
  set workloadPath "workloads/workload_search.tcl"
  set avgFlowSize 1654275
} elseif {[string compare $workload "cachefollower"] == 0} {
  set workloadPath "workloads/workload_cachefollower.tcl"
  set avgFlowSize 701490
} elseif {[string compare $workload "webserver"] == 0} {
  set workloadPath "workloads/workload_webserver.tcl"
  set avgFlowSize 63735
} else {
  puts "Invalid workload: $workload"
  exit 0
}

set overSubscRatio [expr double($numNode/$numTor)/double($numTor/$numAggr)]
set lambda [expr ($numNode*$linkRate*1000000000*$linkLoad)/($avgFlowSize*8.0/$maxPayload*$maxEthernetSize)]
set avgFlowInterval [expr $overSubscRatio/$lambda]

# Random number generators
set RNGFlowSize [new RNG]
$RNGFlowSize seed 61569011

set RNGFlowInterval [new RNG]
$RNGFlowInterval seed 94762103

set RNGSrcNodeId [new RNG]
$RNGSrcNodeId seed 17391005

set RNGDstNodeId [new RNG]
$RNGDstNodeId seed 35010256

set randomFlowSize [new RandomVariable/Empirical]
$randomFlowSize use-rng $RNGFlowSize
$randomFlowSize set interpolation_ 2
$randomFlowSize loadCDF $workloadPath

set randomFlowInterval [new RandomVariable/Exponential]
$randomFlowInterval use-rng $RNGFlowInterval
$randomFlowInterval set avg_ $avgFlowInterval

set randomSrcNodeId [new RandomVariable/Uniform]
$randomSrcNodeId use-rng $RNGSrcNodeId
$randomSrcNodeId set min_ 0
$randomSrcNodeId set max_ $numNode

set randomDstNodeId [new RandomVariable/Uniform]
$randomDstNodeId use-rng $RNGDstNodeId
$randomDstNodeId set min_ 0
$randomDstNodeId set max_ $numNode
set nextTime $simStartTime

puts "Creating flows..."
proc sendBytes {} {
  global ns random_flow_size nextTime mpath_sender_agent mpath_receiver_agent fidx randomFlowSize randomFlowInterval numFlow srcIndex dstIndex flowfile simEndTime
  while {1} {
    set fsize [expr ceil([expr [$randomFlowSize value]])]
    if {$fsize > 0} {
      break;
    }
  }
  puts $flowfile "$nextTime $srcIndex($fidx) $dstIndex($fidx) $fsize"
  #$ns at $simEndTime "$mpath_sender_agent($fidx) close"
  #$ns at $simEndTime "$mpath_receiver_agent($fidx) close"
  set nextTime [expr $nextTime+[$randomFlowInterval value]]
  set fidx [expr $fidx+1]
  if {$fidx < $numFlow} {
    $ns at $nextTime "sendBytes"
  }
}

$ns at 0.0 "puts \"Simulation starts!\""
$ns at $nextTime "sendBytes"
$ns at [expr $simEndTime+1] "finish"
$ns run
