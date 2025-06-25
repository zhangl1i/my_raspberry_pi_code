import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("imu_output.csv")
df.plot(xlabel="Sample Index", ylabel="Angle (deg)", title="IMU Angle Data")
plt.show()