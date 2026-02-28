import pandas as pd
import matplotlib.pyplot as plt

# Load the dataframe from the CSV file
file_name = 'results_sweep.csv' 
try:
    df = pd.read_csv(file_name)
except FileNotFoundError:
    print(f"Error: The file {file_name} was not found.")
    exit()

# Start plotting
plt.figure(figsize=(10, 6))

# Create the line plot
# 'p' is used for the x-axis (p-value)
# 'efficiency' is used for the y-axis (Efficiency)
plt.plot(df['p'], df['efficiency'], 
         marker='o',       # Plot markers for each data point
         linestyle='-')    # Connect the points with a line

# Add titles and labels for clarity
plt.title('Efficiency vs. p-value for CSMA/P-Persistence Simulation')
plt.xlabel('p-value')
plt.ylabel('Efficiency')

# Add a grid for better data visualization
plt.grid(True)

# Save the plot to a file
plt.savefig('p_vs_efficiency_plot.png')

print("Plot saved as p_vs_efficiency_plot.png")