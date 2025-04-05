#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cmath>
#include <vector>
#include <random>

namespace py = pybind11;

// --- Global simulation and physics parameters ---
const double SIM_W = 0.8;   // Half-width for x (x in [-SIM_W, SIM_W])
const double SIM_H = 0.9;   // Height (y in [0, SIM_H])
const double BOTTOM = 0.0;
const double TOP = SIM_H;

const double G_MAG = 0.02 * 0.25;
const double G_ANG = -0.5 * M_PI;
const double G_X = std::cos(G_ANG) * G_MAG;
const double G_Y = std::sin(G_ANG) * G_MAG;

const double SPACING = 0.12;
const double K = SPACING / 1000.0;
const double K_NEAR = K * 10.0;
const double REST_DENSITY = 1.0;
const double RADIUS = SPACING * 1.25;
const double SIGMA = 0.2;
const double MAX_VEL = 2.0;
const double WALL_DAMP = 1.0;
const double VEL_DAMP = 0.5;

// --- Particle class ---
struct Particle {
    double x_pos, y_pos;
    double previous_x_pos, previous_y_pos;
    double visual_x_pos, visual_y_pos;
    double rho, rho_near;
    double press, press_near;
    double x_vel, y_vel;
    double x_force, y_force;
    std::vector<int> neighbors;  // store indices of neighbors

    Particle(double x, double y)
      : x_pos(x), y_pos(y),
        previous_x_pos(x), previous_y_pos(y),
        visual_x_pos(x), visual_y_pos(y),
        rho(0.0), rho_near(0.0),
        press(0.0), press_near(0.0),
        x_vel(0.0), y_vel(0.0),
        x_force(G_X), y_force(G_Y)
    {}

    void update_state(double g_mag, double g_ang) {
        previous_x_pos = x_pos;
        previous_y_pos = y_pos;
        // Euler integration: update velocity from force
        x_vel += x_force;
        y_vel += y_force;
        // Update position
        x_pos += x_vel;
        y_pos += y_vel;
        // Set visual positions
        visual_x_pos = x_pos;
        visual_y_pos = y_pos;
        // Reset forces to gravity vector (polar)
        x_force = std::cos(g_ang) * g_mag;
        y_force = std::sin(g_ang) * g_mag;
        // Recompute velocity from position difference
        x_vel = x_pos - previous_x_pos;
        y_vel = y_pos - previous_y_pos;
        double velocity = std::sqrt(x_vel*x_vel + y_vel*y_vel);
        if (velocity > MAX_VEL) {
            x_vel *= VEL_DAMP;
            y_vel *= VEL_DAMP;
        }
        // Wall constraints
        if (x_pos < -SIM_W) {
            x_force -= (x_pos - (-SIM_W)) * WALL_DAMP;
            visual_x_pos = -SIM_W;
        }
        if (x_pos > SIM_W) {
            x_force -= (x_pos - SIM_W) * WALL_DAMP;
            visual_x_pos = SIM_W;
        }
        if (y_pos < BOTTOM) {
            y_force -= (y_pos - BOTTOM) * WALL_DAMP;
            visual_y_pos = BOTTOM;
        }
        if (y_pos > TOP) {
            y_force -= (y_pos - TOP) * WALL_DAMP;
            visual_y_pos = TOP;
        }
        // Reset densities and neighbor list
        rho = 0.0;
        rho_near = 0.0;
        neighbors.clear();
    }

    void calculate_pressure() {
        press = K * (rho - REST_DENSITY);
        press_near = K_NEAR * rho_near;
    }
};

// --- Simulation class ---
class Simulation {
public:
    std::vector<Particle> particles;

    // Constructor: create "count" particles randomly in [xmin, xmax] x [ymin, ymax]
    Simulation(int count, double xmin, double xmax, double ymin, double ymax) {
        particles.reserve(count);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis_x(xmin, xmax);
        std::uniform_real_distribution<double> dis_y(ymin, ymax);
        for (int i = 0; i < count; i++) {
            double x = dis_x(gen);
            double y = dis_y(gen);
            particles.emplace_back(x, y);
        }
    }

    // Calculate density and near density by looping over particle pairs.
    void calculate_density() {
        int n = particles.size();
        for (int i = 0; i < n; i++) {
            double density = 0.0;
            double density_near = 0.0;
            for (int j = i+1; j < n; j++) {
                double dx = particles[i].x_pos - particles[j].x_pos;
                double dy = particles[i].y_pos - particles[j].y_pos;
                double dist = std::sqrt(dx*dx + dy*dy);
                if (dist < RADIUS) {
                    double q = 1.0 - dist / RADIUS;
                    density += q*q;
                    density_near += q*q*q;
                    particles[j].rho += q*q;
                    particles[j].rho_near += q*q*q;
                    particles[i].neighbors.push_back(j);
                }
            }
            particles[i].rho += density;
            particles[i].rho_near += density_near;
        }
    }

    // Apply pressure forces between particles.
    void create_pressure() {
        int n = particles.size();
        for (int i = 0; i < n; i++) {
            double press_x = 0.0;
            double press_y = 0.0;
            for (int j : particles[i].neighbors) {
                double dx = particles[j].x_pos - particles[i].x_pos;
                double dy = particles[j].y_pos - particles[i].y_pos;
                double dist = std::sqrt(dx*dx + dy*dy);
                if (dist == 0) continue;
                double q = 1.0 - dist / RADIUS;
                double total_pressure = (particles[i].press + particles[j].press) * (q*q)
                    + (particles[i].press_near + particles[j].press_near) * (q*q*q);
                double px = dx * total_pressure / dist;
                double py = dy * total_pressure / dist;
                particles[j].x_force += px;
                particles[j].y_force += py;
                press_x += px;
                press_y += py;
            }
            particles[i].x_force -= press_x;
            particles[i].y_force -= press_y;
        }
    }

    // Apply viscosity forces.
    void calculate_viscosity() {
        int n = particles.size();
        for (int i = 0; i < n; i++) {
            for (int j : particles[i].neighbors) {
                double dx = particles[j].x_pos - particles[i].x_pos;
                double dy = particles[j].y_pos - particles[i].y_pos;
                double dist = std::sqrt(dx*dx + dy*dy);
                if (dist == 0) continue;
                double nx = dx / dist;
                double ny = dy / dist;
                double relative_distance = dist / RADIUS;
                double velocity_diff = (particles[i].x_vel - particles[j].x_vel)*nx +
                                         (particles[i].y_vel - particles[j].y_vel)*ny;
                if (velocity_diff > 0) {
                    double factor = (1.0 - relative_distance) * SIGMA * velocity_diff;
                    double viscosity_x = factor * nx;
                    double viscosity_y = factor * ny;
                    particles[i].x_vel -= viscosity_x * 0.5;
                    particles[i].y_vel -= viscosity_y * 0.5;
                    particles[j].x_vel += viscosity_x * 0.5;
                    particles[j].y_vel += viscosity_y * 0.5;
                }
            }
        }
    }

    // Update one simulation step.
    void update(double g_mag = G_MAG, double g_ang = G_ANG) {
        int n = particles.size();
        // Update state of each particle.
        for (int i = 0; i < n; i++) {
            particles[i].update_state(g_mag, g_ang);
        }
        calculate_density();
        for (int i = 0; i < n; i++) {
            particles[i].calculate_pressure();
        }
        create_pressure();
        calculate_viscosity();
    }

    // Return a flattened vector of visual positions (x0, y0, x1, y1, ...)
    std::vector<double> get_visual_positions() const {
        std::vector<double> pos;
        pos.reserve(particles.size() * 2);
        for (const auto &p : particles) {
            pos.push_back(p.visual_x_pos);
            pos.push_back(p.visual_y_pos);
        }
        return pos;
    }
};

// --- Pybind11 module definition ---
PYBIND11_MODULE(fluidSim, m) {
    m.doc() = "2D SPH fluid simulation module";

    py::class_<Simulation>(m, "Simulation")
        .def(py::init<int, double, double, double, double>(),
             py::arg("count"), py::arg("xmin"), py::arg("xmax"),
             py::arg("ymin"), py::arg("ymax"))
        .def("update", &Simulation::update, py::arg("g_mag") = G_MAG, py::arg("g_ang") = G_ANG)
        .def("get_visual_positions", &Simulation::get_visual_positions);

    // Optionally, expose some of the constants:
    m.attr("SIM_W") = SIM_W;
    m.attr("SIM_H") = SIM_H;
    m.attr("BOTTOM") = BOTTOM;
    m.attr("TOP") = TOP;
    m.attr("G_MAG") = G_MAG;
    m.attr("G_ANG") = G_ANG;
}