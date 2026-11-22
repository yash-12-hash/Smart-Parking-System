# Smart Parking Management and Fee Calculation System

A beginner-friendly, console-based C++ project demonstrating classes,
inheritance, polymorphism, file handling, and time-based fee calculation.

## Features

- Supports bikes, cars, and trucks with different rates and dedicated slots.
- Assigns the first available compatible slot, starting from the nearest floor.
- Records entry and exit times and calculates fees by the started hour:
  - Bike: 20 for the first hour, then 10 per additional hour.
  - Car: 50 for the first hour, then 25 per additional hour.
  - Truck: 100 for the first hour, then 50 per additional hour.
- Prints a receipt when a vehicle checks out.
- Shows the live slot dashboard and searches by vehicle number.
- Saves currently parked vehicles in `active_vehicles.txt`.
- Appends completed visits to `transactions.csv` and reports transaction count
  and revenue.

## Build and run

With g++ installed, open a terminal in this folder and run:

```text
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o parking
parking
```

On Windows, run `parking.exe` after compiling. The two data files are created
in the program's current working directory. Keep them there between runs to
preserve parking and transaction data.

## Parking layout

There are three floors. Each floor has two bike slots, three car slots, and one
truck slot. The allocator checks floors in ascending order and picks the first
empty slot for the selected vehicle type.
