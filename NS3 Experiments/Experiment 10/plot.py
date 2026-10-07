import csv
import os
import matplotlib.pyplot as plt
import numpy as np

# Ensure you have matplotlib installed: pip3 install matplotlib numpy

protocols = ['rip', 'ospf', 'aodv']
scenarios = ['normal', 'congestion', 'failure']

# Initialize data structures for the 3 requested metrics
metrics = {
    'pdr': {'data': {p: [] for p in protocols}, 'title': 'PDR vs. Network Condition', 'ylabel': 'Packet Delivery Ratio (%)', 'file': 'chart_pdr.png'},
    'delay': {'data': {p: [] for p in protocols}, 'title': 'End-to-End Delay vs. Network Condition', 'ylabel': 'Average Delay (ms)', 'file': 'chart_delay.png'},
    'updates': {'data': {p: [] for p in protocols}, 'title': 'Routing Table Updates vs. Network Condition', 'ylabel': 'Number of Table Changes', 'file': 'chart_updates.png'}
}

# 1. Read the CSV files for PDR and Delay
for p in protocols:
    for s in scenarios:
        filepath = f"results/summary_{p}_{s}.csv"
        try:
            with open(filepath, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    metrics['pdr']['data'][p].append(float(row['pdr_percent']))
                    metrics['delay']['data'][p].append(float(row['avg_delay_ms']))
        except FileNotFoundError:
            print(f"Warning: {filepath} not found. Inserting 0.")
            metrics['pdr']['data'][p].append(0.0)
            metrics['delay']['data'][p].append(0.0)

# 2. Parse the rt_*.txt files to count Routing Table Updates
for p in protocols:
    for s in scenarios:
        filepath = f"results/rt_{p}_{s}.txt"
        update_count = 0
        node_tables = {} # Stores the last seen routing table for each node
        
        try:
            with open(filepath, 'r') as f:
                content = f.read()
                
            # NS-3 routing tables output blocks starting with "Node: X"
            blocks = content.split("Node: ")
            for block in blocks[1:]: # Skip the first empty split
                lines = block.strip().split('\n')
                if not lines: continue
                
                # Extract the Node ID from the first line (e.g., "0, Time: +0.25s...")
                first_line = lines[0]
                node_id_str = first_line.split(',')[0].strip()
                
                try:
                    node_id = int(node_id_str)
                except ValueError:
                    continue
                    
                # The remaining lines are the actual routing table entries
                table_state = '\n'.join(lines[1:]).strip()
                
                # Compare current table state to the last known state for this node
                if node_id not in node_tables:
                    node_tables[node_id] = table_state
                    update_count += 1  # Initial population counts as an update
                elif node_tables[node_id] != table_state:
                    node_tables[node_id] = table_state
                    update_count += 1  # Route added, removed, or metric changed
                    
            metrics['updates']['data'][p].append(update_count)
            
        except FileNotFoundError:
            print(f"Warning: {filepath} not found. Inserting 0.")
            metrics['updates']['data'][p].append(0)

# 3. Generate and save the plots
x = np.arange(len(scenarios))
width = 0.25
colors = ['#1f77b4', '#ff7f0e', '#2ca02c'] # Blue, Orange, Green

for key, metric in metrics.items():
    plt.figure(figsize=(9, 6))
    
    # Create grouped bars
    plt.bar(x - width, metric['data']['rip'], width, label='RIP', color=colors[0], edgecolor='black', zorder=3)
    plt.bar(x, metric['data']['ospf'], width, label='OSPF', color=colors[1], edgecolor='black', zorder=3)
    plt.bar(x + width, metric['data']['aodv'], width, label='AODV', color=colors[2], edgecolor='black', zorder=3)
    
    # Formatting
    plt.title(metric['title'], fontsize=14, fontweight='bold')
    plt.xlabel('Network Condition', fontsize=12, fontweight='bold')
    plt.ylabel(metric['ylabel'], fontsize=12, fontweight='bold')
    
    # Label the X-axis with the scenarios
    plt.xticks(x, [s.capitalize() for s in scenarios], fontsize=11)
    
    plt.legend()
    plt.grid(axis='y', linestyle='--', alpha=0.7, zorder=0)
    
    # Save to the results folder
    out_file = os.path.join("results", metric['file'])
    plt.savefig(out_file, bbox_inches='tight', dpi=300)
    print(f"Saved plot: {out_file}")

# Display the graphs on your screen
plt.show()
