#!/usr/bin/env python3
"""
Plot CDF of message latency from ns-3 FlowMonitor output.
Usage: python3 plot_latency_cdf.py incast-udp.flowmon
"""

import xml.etree.ElementTree as ET
import matplotlib.pyplot as plt
import sys

def parse_flowmon_for_cdf(flowmon_file):
    """Parse flowmon and extract probe latency distribution for CDF."""
    tree = ET.parse(flowmon_file)
    root = tree.getroot()
    
    # Find probe flows (port 1235 - echo traffic)
    for flow in root.findall('.//FlowStats/Flow'):
        flow_id = flow.get('flowId')
        delay_hist = flow.find('delayHistogram')
        
        if delay_hist is None or len(delay_hist.findall('bin')) == 0:
            continue
            
        # Get classifier info to identify probe traffic
        classifier = root.find(f'.//Ipv4FlowClassifier/Flow[@flowId="{flow_id}"]')
        if classifier is not None:
            dst_port = classifier.get('destinationPort')
            if dst_port == "1235":  # Probe traffic to echo server
                print(f"\n=== Flow {flow_id} (Probe Latency) ===")
                
                bins = []
                counts = []
                
                for bin_elem in delay_hist.findall('bin'):
                    start = float(bin_elem.get('start')) * 1e6  # Convert to μs
                    width = float(bin_elem.get('width')) * 1e6
                    count = int(bin_elem.get('count'))
                    
                    bins.append((start, start + width))
                    counts.append(count)
                    print(f"  {start:.1f}-{start+width:.1f} μs: {count} packets")
                
                # Calculate CDF
                total = sum(counts)
                cumsum = [sum(counts[:i+1]) for i in range(len(counts))]
                cdf = [c / total for c in cumsum]
                
                # Prepare data for plotting (step function)
                bin_edges = [b[0] for b in bins]
                bin_edges.append(bins[-1][1])
                
                return bin_edges, [0] + cdf, total
    
    return None, None, 0

def plot_cdf(bin_edges, cdf_values, total_packets, output_dir):
    """Plot the latency CDF."""
    plt.figure(figsize=(10, 6))
    plt.step(bin_edges, cdf_values, where='post', linewidth=2, label=f'Probe Latency (n={total_packets})')
    
    plt.xlabel('Latency (μs)', fontsize=12)
    plt.ylabel('CDF', fontsize=12)
    plt.title('Message Latency CDF under UDP Incast', fontsize=14)
    plt.grid(True, alpha=0.3)
    plt.legend()
    
    # X-axis: start from 0, end 20% after max latency
    max_latency = bin_edges[-1]
    plt.xlim(0, max_latency * 1.2)
    
    # Y-axis: 0 to 1.05 to show CDF=1 clearly
    plt.ylim(0, 1.05)
    
    # Add statistics text
    stats_text = f"Total packets: {total_packets}\n"
    if len(cdf_values) > 1:
        # Find percentiles
        for p, label in [(0.5, '50th'), (0.9, '90th'), (0.99, '99th')]:
            for i, cdf in enumerate(cdf_values):
                if cdf >= p:
                    stats_text += f"{label} percentile: ~{bin_edges[i]:.1f}μs\n"
                    break
    
    plt.text(0.05, 0.95, stats_text, transform=plt.gca().transAxes, 
             verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    # Save to script's directory
    import os
    output_path = os.path.join(output_dir, 'latency_cdf.png')
    plt.savefig(output_path, dpi=150)
    print(f"\nCDF saved to {output_path}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <flowmon_file>")
        sys.exit(1)
    
    import os
    flowmon_file = sys.argv[1]
    
    # Get directory of the flowmon file (where .cc files are located)
    output_dir = os.path.dirname(os.path.abspath(flowmon_file))
    
    bin_edges, cdf_values, total = parse_flowmon_for_cdf(flowmon_file)
    
    if bin_edges and cdf_values:
        plot_cdf(bin_edges, cdf_values, total, output_dir)
    else:
        print("No probe latency data found in flowmon file.")
