#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum class VehicleType {
    Bike,
    Car,
    Truck
};

std::string typeName(VehicleType type) {
    switch (type) {
    case VehicleType::Bike:
        return "Bike";
    case VehicleType::Car:
        return "Car";
    case VehicleType::Truck:
        return "Truck";
    }
    return "Unknown";
}

class Vehicle {
public:
    explicit Vehicle(std::string plate) : plate_(std::move(plate)) {}
    virtual ~Vehicle() = default;

    const std::string& plate() const { return plate_; }
    virtual VehicleType type() const = 0;
    virtual double firstHourFee() const = 0;
    virtual double extraHourFee() const = 0;

private:
    std::string plate_;
};

class Bike : public Vehicle {
public:
    using Vehicle::Vehicle;
    VehicleType type() const override { return VehicleType::Bike; }
    double firstHourFee() const override { return 20.0; }
    double extraHourFee() const override { return 10.0; }
};

class Car : public Vehicle {
public:
    using Vehicle::Vehicle;
    VehicleType type() const override { return VehicleType::Car; }
    double firstHourFee() const override { return 50.0; }
    double extraHourFee() const override { return 25.0; }
};

class Truck : public Vehicle {
public:
    using Vehicle::Vehicle;
    VehicleType type() const override { return VehicleType::Truck; }
    double firstHourFee() const override { return 100.0; }
    double extraHourFee() const override { return 50.0; }
};

std::unique_ptr<Vehicle> makeVehicle(const std::string& type,
                                     const std::string& plate);

struct ParkingSlot {
    int floor;
    std::string number;
    VehicleType type;
    std::string ticketId;
};

struct ParkingRecord {
    std::string ticketId;
    std::unique_ptr<Vehicle> vehicle;
    int floor;
    std::string slotNumber;
    std::time_t entryTime;
};

class ParkingLot {
public:
    ParkingLot() {
        addFloor(1, 'A');
        addFloor(2, 'B');
        addFloor(3, 'C');
    }

    bool loadActiveVehicles() {
        std::ifstream file("active_vehicles.txt");
        if (!file) {
            std::cerr << "Could not read active_vehicles.txt.\n";
            return false;
        }

        std::string line;
        int lineNumber = 0;
        while (std::getline(file, line)) {
            ++lineNumber;
            if (line.empty()) {
                continue;
            }

            std::istringstream row(line);
            std::string ticketId;
            std::string plate;
            std::string type;
            std::string floorText;
            std::string slotNumber;
            std::string entryText;
            std::string extra;

            if (!std::getline(row, ticketId, '|') ||
                !std::getline(row, plate, '|') ||
                !std::getline(row, type, '|') ||
                !std::getline(row, floorText, '|') ||
                !std::getline(row, slotNumber, '|') ||
                !std::getline(row, entryText, '|') ||
                std::getline(row, extra, '|')) {
                std::cerr << "Invalid data in active_vehicles.txt at line "
                          << lineNumber << ".\n";
                return false;
            }

            int floor = 0;
            std::time_t entryTime = 0;
            try {
                floor = std::stoi(floorText);
                entryTime = static_cast<std::time_t>(std::stoll(entryText));
            } catch (const std::exception&) {
                std::cerr << "Invalid data in active_vehicles.txt at line "
                          << lineNumber << ".\n";
                return false;
            }

            std::unique_ptr<Vehicle> vehicle = makeVehicle(type, plate);
            ParkingSlot* slot = findSlot(floor, slotNumber);
            if (!vehicle || !slot || slot->type != vehicle->type() ||
                !slot->ticketId.empty() || findByPlate(plate) ||
                ticketExists(ticketId)) {
                std::cerr << "Conflicting or unsupported active parking record "
                          << "at line " << lineNumber << ".\n";
                return false;
            }

            slot->ticketId = ticketId;
            records_.push_back({ticketId, std::move(vehicle), floor, slotNumber,
                                entryTime});
            updateNextTicketNumber(ticketId);
        }

        if (file.bad()) {
            std::cerr << "Could not read active_vehicles.txt.\n";
            return false;
        }
        return true;
    }

    bool loadTicketSequenceFromTransactions() {
        std::ifstream file("transactions.csv");
        if (!file) {
            std::cerr << "Could not read transactions.csv.\n";
            return false;
        }

        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            const std::size_t comma = line.find(',');
            if (comma != std::string::npos) {
                updateNextTicketNumber(line.substr(0, comma));
            }
        }
        if (file.bad()) {
            std::cerr << "Could not read transactions.csv.\n";
            return false;
        }
        return true;
    }

    bool saveActiveVehicles() const {
        std::ofstream file("active_vehicles.txt", std::ios::trunc);
        if (!file) {
            std::cerr << "Could not write active_vehicles.txt.\n";
            return false;
        }

        for (const ParkingRecord& record : records_) {
            file << record.ticketId << '|' << record.vehicle->plate() << '|'
                 << typeName(record.vehicle->type()) << '|' << record.floor
                 << '|' << record.slotNumber << '|' << record.entryTime << '\n';
        }
        if (!file) {
            std::cerr << "Could not finish writing active_vehicles.txt.\n";
            return false;
        }
        return true;
    }

    ParkingRecord* findByPlate(const std::string& plate) {
        for (ParkingRecord& record : records_) {
            if (record.vehicle->plate() == plate) {
                return &record;
            }
        }
        return nullptr;
    }

    ParkingRecord* findByTicket(const std::string& ticketId) {
        for (ParkingRecord& record : records_) {
            if (record.ticketId == ticketId) {
                return &record;
            }
        }
        return nullptr;
    }

    bool ticketExists(const std::string& ticketId) const {
        return std::any_of(records_.begin(), records_.end(),
                           [&ticketId](const ParkingRecord& record) {
                               return record.ticketId == ticketId;
                           });
    }

    std::string nextTicketId() {
        std::ostringstream ticket;
        ticket << 'T' << std::setw(4) << std::setfill('0') << nextTicketNumber_++;
        return ticket.str();
    }

    ParkingSlot* nearestAvailableSlot(VehicleType type) {
        for (ParkingSlot& slot : slots_) {
            if (slot.type == type && slot.ticketId.empty()) {
                return &slot;
            }
        }
        return nullptr;
    }

    void addRecord(ParkingRecord record) {
        ParkingSlot* slot = findSlot(record.floor, record.slotNumber);
        if (slot) {
            slot->ticketId = record.ticketId;
            records_.push_back(std::move(record));
        }
    }

    bool removeRecord(const std::string& ticketId, ParkingRecord& removed) {
        for (auto it = records_.begin(); it != records_.end(); ++it) {
            if (it->ticketId == ticketId) {
                ParkingSlot* slot = findSlot(it->floor, it->slotNumber);
                if (slot) {
                    slot->ticketId.clear();
                }
                removed = std::move(*it);
                records_.erase(it);
                return true;
            }
        }
        return false;
    }

    const std::vector<ParkingSlot>& slots() const { return slots_; }
    const std::vector<ParkingRecord>& records() const { return records_; }

private:
    void addFloor(int floor, char prefix) {
        for (int i = 1; i <= 2; ++i) {
            slots_.push_back({floor, std::string(1, prefix) + std::to_string(i),
                              VehicleType::Bike, ""});
        }
        for (int i = 3; i <= 5; ++i) {
            slots_.push_back({floor, std::string(1, prefix) + std::to_string(i),
                              VehicleType::Car, ""});
        }
        slots_.push_back({floor, std::string(1, prefix) + "6",
                          VehicleType::Truck, ""});
    }

    ParkingSlot* findSlot(int floor, const std::string& number) {
        for (ParkingSlot& slot : slots_) {
            if (slot.floor == floor && slot.number == number) {
                return &slot;
            }
        }
        return nullptr;
    }

    void updateNextTicketNumber(const std::string& ticketId) {
        if (ticketId.size() < 2 || ticketId[0] != 'T') {
            return;
        }
        try {
            const int number = std::stoi(ticketId.substr(1));
            if (number >= nextTicketNumber_) {
                nextTicketNumber_ = number + 1;
            }
        } catch (const std::exception&) {
            // A nonstandard ticket ID is still usable; it does not affect numbering.
        }
    }

    std::vector<ParkingSlot> slots_;
    std::vector<ParkingRecord> records_;
    int nextTicketNumber_ = 1;
};

std::unique_ptr<Vehicle> makeVehicle(const std::string& type,
                                     const std::string& plate) {
    if (type == "Bike") {
        return std::make_unique<Bike>(plate);
    }
    if (type == "Car") {
        return std::make_unique<Car>(plate);
    }
    if (type == "Truck") {
        return std::make_unique<Truck>(plate);
    }
    return nullptr;
}

std::string formatTime(std::time_t time) {
    const std::tm* local = std::localtime(&time);
    if (!local) {
        return "Unknown";
    }
    std::ostringstream output;
    output << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

long long calculateDuration(std::time_t entry, std::time_t exit) {
    return std::max(0LL, static_cast<long long>(exit) -
                             static_cast<long long>(entry));
}

double calculateFee(const Vehicle& vehicle, long long durationSeconds) {
    const long long extraSeconds = std::max(0LL, durationSeconds - 3600);
    const long long extraHours = (extraSeconds + 3599) / 3600;
    return vehicle.firstHourFee() + extraHours * vehicle.extraHourFee();
}

std::string formatDuration(long long seconds) {
    const long long hours = seconds / 3600;
    const long long minutes = (seconds % 3600) / 60;
    const long long remainingSeconds = seconds % 60;
    std::ostringstream output;
    output << hours << "h " << minutes << "m " << remainingSeconds << "s";
    return output.str();
}

bool validPlate(const std::string& plate) {
    if (plate.empty()) {
        return false;
    }
    return std::all_of(plate.begin(), plate.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-';
    });
}

bool appendTransaction(const ParkingRecord& record, std::time_t exitTime,
                       long long durationSeconds, double fee) {
    const bool needsHeader = [] {
        std::ifstream existing("transactions.csv");
        return !existing.good() || existing.peek() == std::ifstream::traits_type::eof();
    }();

    std::ofstream file("transactions.csv", std::ios::app);
    if (!file) {
        std::cerr << "Could not write transactions.csv.\n";
        return false;
    }
    if (needsHeader) {
        file << "Ticket ID,Vehicle Number,Type,Floor,Slot,Entry Time,Exit Time,"
                "Duration Seconds,Fee\n";
    }
    file << record.ticketId << ',' << record.vehicle->plate() << ','
         << typeName(record.vehicle->type()) << ',' << record.floor << ','
         << record.slotNumber << ',' << formatTime(record.entryTime) << ','
         << formatTime(exitTime) << ',' << durationSeconds << ','
         << std::fixed << std::setprecision(2) << fee << '\n';
    if (!file) {
        std::cerr << "Could not finish writing transactions.csv.\n";
        return false;
    }
    return true;
}

bool readInteger(const std::string& prompt, int& value) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    std::istringstream parser(input);
    char extra;
    if (!(parser >> value) || (parser >> extra)) {
        std::cout << "Please enter a valid number.\n";
        return false;
    }
    return true;
}

void printReceipt(const ParkingRecord& record, std::time_t exitTime,
                  long long durationSeconds, double fee) {
    std::cout << "\n========== PARKING RECEIPT ==========\n"
              << "Ticket ID:     " << record.ticketId << '\n'
              << "Vehicle:       " << record.vehicle->plate() << " ("
              << typeName(record.vehicle->type()) << ")\n"
              << "Slot:          Floor " << record.floor << ", "
              << record.slotNumber << '\n'
              << "Entry time:    " << formatTime(record.entryTime) << '\n'
              << "Exit time:     " << formatTime(exitTime) << '\n'
              << "Duration:      " << formatDuration(durationSeconds) << '\n'
              << "Total fee:     " << std::fixed << std::setprecision(2)
              << fee << '\n'
              << "=====================================\n";
}

void showDashboard(const ParkingLot& lot) {
    std::cout << "\n========== LIVE PARKING DASHBOARD ==========\n";
    int available = 0;
    for (const ParkingSlot& slot : lot.slots()) {
        const bool occupied = !slot.ticketId.empty();
        if (!occupied) {
            ++available;
        }
        std::cout << "Floor " << slot.floor << " | Slot " << std::setw(2)
                  << std::left << slot.number << " | " << std::setw(5)
                  << typeName(slot.type) << " | "
                  << (occupied ? "Occupied (" + slot.ticketId + ")" : "Empty")
                  << '\n';
    }
    std::cout << "Active vehicles: " << lot.records().size()
              << " | Available slots: " << available << '\n';
}

void showAnalytics() {
    std::ifstream file("transactions.csv");
    if (!file) {
        std::cout << "\nNo completed transactions have been recorded yet.\n";
        return;
    }

    std::string line;
    std::getline(file, line);
    int transactionCount = 0;
    double totalRevenue = 0.0;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const std::size_t comma = line.rfind(',');
        if (comma == std::string::npos) {
            std::cerr << "Skipping an invalid transaction log row.\n";
            continue;
        }
        try {
            totalRevenue += std::stod(line.substr(comma + 1));
            ++transactionCount;
        } catch (const std::exception&) {
            std::cerr << "Skipping an invalid transaction log fee.\n";
        }
    }
    if (file.bad()) {
        std::cerr << "Could not read transactions.csv.\n";
        return;
    }
    std::cout << "\n========== ADMIN ANALYTICS ==========\n"
              << "Completed transactions: " << transactionCount << '\n'
              << "Total revenue:          " << std::fixed
              << std::setprecision(2) << totalRevenue << '\n';
}

bool parkVehicle(ParkingLot& lot) {
    std::cout << "\nVehicle type: 1. Bike  2. Car  3. Truck\n";
    int choice = 0;
    if (!readInteger("Choose type: ", choice) || choice < 1 || choice > 3) {
        std::cout << "Invalid vehicle type.\n";
        return false;
    }

    std::string plate;
    std::cout << "Enter vehicle number (letters, digits, hyphens): ";
    std::getline(std::cin, plate);
    std::transform(plate.begin(), plate.end(), plate.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    if (!validPlate(plate)) {
        std::cout << "Vehicle number is invalid.\n";
        return false;
    }
    if (lot.findByPlate(plate)) {
        std::cout << "That vehicle is already parked.\n";
        return false;
    }

    const std::string type = choice == 1 ? "Bike" : (choice == 2 ? "Car" : "Truck");
    std::unique_ptr<Vehicle> vehicle = makeVehicle(type, plate);
    ParkingSlot* slot = lot.nearestAvailableSlot(vehicle->type());
    if (!slot) {
        std::cout << "No available " << type << " slots.\n";
        return false;
    }

    ParkingRecord record{lot.nextTicketId(), std::move(vehicle), slot->floor,
                         slot->number, std::time(nullptr)};
    const std::string ticketId = record.ticketId;
    lot.addRecord(std::move(record));
    if (!lot.saveActiveVehicles()) {
        ParkingRecord rollback;
        lot.removeRecord(ticketId, rollback);
        return false;
    }

    ParkingRecord* parked = lot.findByTicket(ticketId);
    std::cout << "\nVehicle parked successfully.\n"
              << "Ticket ID: " << ticketId << '\n'
              << "Slot: Floor " << parked->floor << ", " << parked->slotNumber
              << '\n'
              << "Entry time: " << formatTime(parked->entryTime) << '\n';
    return true;
}

bool checkOutVehicle(ParkingLot& lot) {
    std::string plate;
    std::cout << "\nEnter vehicle number: ";
    std::getline(std::cin, plate);
    std::transform(plate.begin(), plate.end(), plate.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });

    ParkingRecord* current = lot.findByPlate(plate);
    if (!current) {
        std::cout << "No parked vehicle found with that number.\n";
        return false;
    }

    const std::string ticketId = current->ticketId;
    ParkingRecord removed;
    if (!lot.removeRecord(ticketId, removed)) {
        std::cerr << "Could not check out the vehicle.\n";
        return false;
    }
    if (!lot.saveActiveVehicles()) {
        lot.addRecord(std::move(removed));
        return false;
    }

    const std::time_t exitTime = std::time(nullptr);
    const long long duration = calculateDuration(removed.entryTime, exitTime);
    const double fee = calculateFee(*removed.vehicle, duration);
    if (!appendTransaction(removed, exitTime, duration, fee)) {
        lot.addRecord(std::move(removed));
        lot.saveActiveVehicles();
        return false;
    }

    printReceipt(removed, exitTime, duration, fee);
    return true;
}

void searchVehicle(ParkingLot& lot) {
    std::string plate;
    std::cout << "\nEnter vehicle number: ";
    std::getline(std::cin, plate);
    std::transform(plate.begin(), plate.end(), plate.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    const ParkingRecord* record = lot.findByPlate(plate);
    if (!record) {
        std::cout << "No parked vehicle found with that number.\n";
        return;
    }
    const long long duration = calculateDuration(record->entryTime, std::time(nullptr));
    std::cout << "\nVehicle found.\n"
              << "Ticket ID: " << record->ticketId << '\n'
              << "Type:      " << typeName(record->vehicle->type()) << '\n'
              << "Slot:      Floor " << record->floor << ", "
              << record->slotNumber << '\n'
              << "Entry:     " << formatTime(record->entryTime) << '\n'
              << "Parked for: " << formatDuration(duration) << '\n';
}

int main() {
    std::ofstream activeFile("active_vehicles.txt", std::ios::app);
    if (!activeFile) {
        std::cerr << "Could not create or open active_vehicles.txt.\n";
        return 1;
    }
    activeFile.close();
    std::ofstream transactionFile("transactions.csv", std::ios::app);
    if (!transactionFile) {
        std::cerr << "Could not create or open transactions.csv.\n";
        return 1;
    }
    transactionFile.close();

    ParkingLot lot;
    if (!lot.loadActiveVehicles() || !lot.loadTicketSequenceFromTransactions()) {
        return 1;
    }

    std::cout << "Welcome to Smart Parking Management System\n";
    bool running = true;
    while (running && std::cin) {
        std::cout << "\n1. Park a vehicle\n"
                  << "2. Check out a vehicle\n"
                  << "3. View live dashboard\n"
                  << "4. Search parked vehicle\n"
                  << "5. Admin analytics\n"
                  << "0. Save and exit\n";
        int choice = -1;
        if (!readInteger("Select an option: ", choice)) {
            continue;
        }

        switch (choice) {
        case 1:
            parkVehicle(lot);
            break;
        case 2:
            checkOutVehicle(lot);
            break;
        case 3:
            showDashboard(lot);
            break;
        case 4:
            searchVehicle(lot);
            break;
        case 5:
            showAnalytics();
            break;
        case 0:
            running = false;
            std::cout << "Goodbye.\n";
            break;
        default:
            std::cout << "Choose an option from the menu.\n";
        }
    }
    return 0;
}
