#include <gtk/gtk.h>
#include <math.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    DRIVE_MODE_ECO,
    DRIVE_MODE_NORMAL,
    DRIVE_MODE_SPORT
} DriveMode;

typedef struct {
    double battery_voltage;      // V
    double battery_capacity;    // kWh
    double motor_power;         // kW
    double motor_torque;        // Nm
    double motor_rpm;           // RPM
    double vehicle_speed;       // km/h
    double acceleration;        // m/s²
    double soc;                // State of Charge (%)
    double distance;           // km
    double energy_consumed;    // kWh
    double regen_efficiency;   // 0.0 to 1.0
    double battery_temp;       // °C
    double energy_efficiency;  // Wh/km
    DriveMode drive_mode;
    gboolean is_running;
    gboolean regen_braking;
} EVSimulation;

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    GtkWidget *reset_button;
    GtkWidget *battery_voltage_entry;
    GtkWidget *battery_capacity_entry;
    GtkWidget *motor_power_entry;
    GtkWidget *regen_braking_switch;
    GtkWidget *regen_efficiency_scale;
    GtkWidget *drive_mode_dropdown;
    GtkWidget *speed_label;
    GtkWidget *soc_label;
    GtkWidget *distance_label;
    GtkWidget *energy_label;
    GtkWidget *torque_label;
    GtkWidget *rpm_label;
    GtkWidget *temp_label;
    GtkWidget *efficiency_label;
    GtkWidget *accel_spin;
} AppWidgets;

EVSimulation sim_data = {
    .battery_voltage = 400,
    .battery_capacity = 60,
    .motor_power = 150,
    .motor_torque = 0,
    .motor_rpm = 0,
    .vehicle_speed = 0,
    .acceleration = 0,
    .soc = 100,
    .distance = 0,
    .energy_consumed = 0,
    .regen_efficiency = 0.5,
    .battery_temp = 25.0,
    .energy_efficiency = 0,
    .drive_mode = DRIVE_MODE_NORMAL,
    .is_running = FALSE,
    .regen_braking = FALSE
};

#define WAVE_POINTS 200
double voltage_wave[WAVE_POINTS];
double current_wave[WAVE_POINTS];
double speed_wave[WAVE_POINTS];
double temp_wave[WAVE_POINTS];
int wave_index = 0;
guint32 last_time = 0;

double parse_input(const char *text, double min, double max, double default_val) {
    double val = atof(text);
    return (isnan(val) || val < min || val > max) ? default_val : val;
}

void update_waveforms() {
    if (last_time == 0) {
        last_time = g_get_monotonic_time();
    }
    guint32 current_time = g_get_monotonic_time();
    double time_diff = (current_time - last_time) / 1000000.0; // seconds    
    voltage_wave[wave_index] = sim_data.battery_voltage * (0.95 + 0.05 * sin(time_diff * 0.01));
    current_wave[wave_index] = (sim_data.motor_power * 1000 / sim_data.battery_voltage) * 
                              (0.9 + 0.1 * sin(time_diff * 0.02));
    speed_wave[wave_index] = sim_data.vehicle_speed;
    temp_wave[wave_index] = sim_data.battery_temp;
    wave_index = (wave_index + 1) % WAVE_POINTS;
}

static void draw_waveforms(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);    
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 0.5);
    for (int i = 0; i <= 10; i++) {
        cairo_move_to(cr, 0, i * height / 10);
        cairo_line_to(cr, width, i * height / 10);
        cairo_move_to(cr, i * width / 10, 0);
        cairo_line_to(cr, i * width / 10, height);
    }
    cairo_stroke(cr);
    
    /// Voltage waveform (red)
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < WAVE_POINTS; i++) {
        int idx = (wave_index + i) % WAVE_POINTS;
        double x = (double)i / WAVE_POINTS * width;
        double y = height - (voltage_wave[idx] / (sim_data.battery_voltage * 1.2) * height * 0.8);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
	/// Current waveform (green)
    double max_current = (sim_data.motor_power * 1000 / sim_data.battery_voltage) * 1.2;
    cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < WAVE_POINTS; i++) {
        int idx = (wave_index + i) % WAVE_POINTS;
        double x = (double)i / WAVE_POINTS * width;
        double y = height - (current_wave[idx] / max_current * height * 0.8);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    /// Speed waveform (blue)
    cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < WAVE_POINTS; i++) {
        int idx = (wave_index + i) % WAVE_POINTS;
        double x = (double)i / WAVE_POINTS * width;
        double y = height - (speed_wave[idx] / 200 * height * 0.8);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    /// Temperature waveform (yellow)
    cairo_set_source_rgb(cr, 1.0, 1.0, 0.2);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < WAVE_POINTS; i++) {
        int idx = (wave_index + i) % WAVE_POINTS;
        double x = (double)i / WAVE_POINTS * width;
        double y = height - ((temp_wave[idx] - 10) / 60 * height * 0.8); // Scale 10°C to 70°C
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
    cairo_move_to(cr, 10, 20);
    cairo_show_text(cr, "Voltage (V)");
    cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);
    cairo_move_to(cr, 10, 40);
    cairo_show_text(cr, "Current (A)");
    cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
    cairo_move_to(cr, 10, 60);
    cairo_show_text(cr, "Speed (km/h)");
    cairo_set_source_rgb(cr, 1.0, 1.0, 0.2);
    cairo_move_to(cr, 10, 80);
    cairo_show_text(cr, "Temp (°C)");
}

static gboolean update_simulation(gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;    
    if (sim_data.is_running) {
        static guint32 last_update = 0;
        guint32 now = g_get_monotonic_time();
        double dt = (last_update == 0) ? 0.2 : (now - last_update) / 1000000.0;
        last_update = now;
        double mass = 1500;
        double drag_coeff = 0.3;
        double frontal_area = 2.5;
        double air_density = 1.225;
        double rolling_resistance = 0.01;
        double max_accel;
        double power_factor;
        switch (sim_data.drive_mode) {
            case DRIVE_MODE_ECO:
                max_accel = 0.5;
                power_factor = 0.7;
                break;
            case DRIVE_MODE_NORMAL:
                max_accel = 1.0;
                power_factor = 1.0;
                break;
            case DRIVE_MODE_SPORT:
                max_accel = 1.5;
                power_factor = 1.3;
                break;
        }
        sim_data.acceleration = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets->accel_spin));
        if (sim_data.acceleration > max_accel) sim_data.acceleration = max_accel;
        if (sim_data.acceleration < -max_accel) sim_data.acceleration = -max_accel;
        double speed_ms = sim_data.vehicle_speed / 3.6;
        double force = mass * sim_data.acceleration;
        double drag = 0.5 * drag_coeff * frontal_area * air_density * speed_ms * speed_ms;
        double rolling = rolling_resistance * mass * 9.81;
        double total_force = force - drag - rolling;
        speed_ms += (total_force / mass) * dt;
        sim_data.vehicle_speed = speed_ms * 3.6;
        if (sim_data.vehicle_speed < 0) sim_data.vehicle_speed = 0;
        if (sim_data.vehicle_speed > 180) sim_data.vehicle_speed = 180;
        sim_data.motor_rpm = sim_data.vehicle_speed * 50;
        sim_data.motor_torque = sim_data.motor_power * power_factor * 1000 / 
                              (sim_data.motor_rpm / 60 * 2 * M_PI + 0.1);
        sim_data.distance += sim_data.vehicle_speed / 3600 * dt;
        double temp_efficiency = 1.0 - (sim_data.battery_temp > 40 ? (sim_data.battery_temp - 40) * 0.01 : 0);
        double power_use = sim_data.motor_power * power_factor * 
                          (0.5 + 0.5 * fabs(sim_data.acceleration)) / (0.85 * temp_efficiency);
        sim_data.energy_consumed += power_use / 3600 * dt;
        sim_data.soc = 100 - (sim_data.energy_consumed / sim_data.battery_capacity * 100);
        if (sim_data.soc < 0) sim_data.soc = 0;
        if (sim_data.acceleration < 0 && sim_data.regen_braking) {   /// Regenerative braking
            double regen_energy = sim_data.regen_efficiency * power_use * 0.5;
            sim_data.energy_consumed -= regen_energy / 3600 * dt;
            sim_data.soc = 100 - (sim_data.energy_consumed / sim_data.battery_capacity * 100);
            if (sim_data.soc > 100) sim_data.soc = 100;
        }
        sim_data.battery_temp += (power_use / sim_data.motor_power) * 0.1 * dt;
        sim_data.battery_temp -= 0.05 * dt; /// Cooling effect
        if (sim_data.battery_temp < 10) sim_data.battery_temp = 10;
        if (sim_data.battery_temp > 70) sim_data.battery_temp = 70;
        if (sim_data.distance > 0) {  /// Calculate energy efficiency
            sim_data.energy_efficiency = (sim_data.energy_consumed * 1000) / sim_data.distance;
        } else {
            sim_data.energy_efficiency = 0;
        }
        update_waveforms();
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f km/h", sim_data.vehicle_speed);
        gtk_label_set_text(GTK_LABEL(widgets->speed_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.1f %%", sim_data.soc);
        gtk_label_set_text(GTK_LABEL(widgets->soc_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.2f km", sim_data.distance);
        gtk_label_set_text(GTK_LABEL(widgets->distance_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.2f kWh", sim_data.energy_consumed);
        gtk_label_set_text(GTK_LABEL(widgets->energy_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.1f Nm", sim_data.motor_torque);
        gtk_label_set_text(GTK_LABEL(widgets->torque_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.0f RPM", sim_data.motor_rpm);
        gtk_label_set_text(GTK_LABEL(widgets->rpm_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.1f °C", sim_data.battery_temp);
        gtk_label_set_text(GTK_LABEL(widgets->temp_label), buffer);
        snprintf(buffer, sizeof(buffer), "%.0f Wh/km", sim_data.energy_efficiency);
        gtk_label_set_text(GTK_LABEL(widgets->efficiency_label), buffer);
        gtk_widget_queue_draw(widgets->drawing_area);
    }
    return G_SOURCE_CONTINUE;
}

static void start_simulation(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;    
    const char *voltage_text = gtk_editable_get_text(GTK_EDITABLE(widgets->battery_voltage_entry));
    const char *capacity_text = gtk_editable_get_text(GTK_EDITABLE(widgets->battery_capacity_entry));
    const char *power_text = gtk_editable_get_text(GTK_EDITABLE(widgets->motor_power_entry));
    sim_data.battery_voltage = parse_input(voltage_text, 100, 1000, 400);
    sim_data.battery_capacity = parse_input(capacity_text, 10, 200, 60);
    sim_data.motor_power = parse_input(power_text, 50, 500, 150);
    sim_data.regen_braking = gtk_switch_get_active(GTK_SWITCH(widgets->regen_braking_switch));
    sim_data.regen_efficiency = gtk_range_get_value(GTK_RANGE(widgets->regen_efficiency_scale)) / 100.0;
    sim_data.drive_mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(widgets->drive_mode_dropdown));
    sim_data.vehicle_speed = 0;
    sim_data.motor_rpm = 0;
    sim_data.motor_torque = 0;
    sim_data.distance = 0;
    sim_data.energy_consumed = 0;
    sim_data.soc = 100;
    sim_data.battery_temp = 25.0;
    sim_data.energy_efficiency = 0;
    last_time = 0;
    sim_data.is_running = TRUE;
    gtk_widget_set_sensitive(widgets->start_button, FALSE);
    gtk_widget_set_sensitive(widgets->stop_button, TRUE);
    gtk_widget_set_sensitive(widgets->reset_button, TRUE);
    gtk_widget_set_sensitive(widgets->battery_voltage_entry, FALSE);
    gtk_widget_set_sensitive(widgets->battery_capacity_entry, FALSE);
    gtk_widget_set_sensitive(widgets->motor_power_entry, FALSE);
    gtk_widget_set_sensitive(widgets->regen_braking_switch, FALSE);
    gtk_widget_set_sensitive(widgets->regen_efficiency_scale, FALSE);
    gtk_widget_set_sensitive(widgets->drive_mode_dropdown, FALSE);
    gtk_widget_set_sensitive(widgets->accel_spin, TRUE);
}

static void stop_simulation(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;    
    sim_data.is_running = FALSE;
    gtk_widget_set_sensitive(widgets->start_button, TRUE);
    gtk_widget_set_sensitive(widgets->stop_button, FALSE);
    gtk_widget_set_sensitive(widgets->reset_button, FALSE);
    gtk_widget_set_sensitive(widgets->battery_voltage_entry, TRUE);
    gtk_widget_set_sensitive(widgets->battery_capacity_entry, TRUE);
    gtk_widget_set_sensitive(widgets->motor_power_entry, TRUE);
    gtk_widget_set_sensitive(widgets->regen_braking_switch, TRUE);
    gtk_widget_set_sensitive(widgets->regen_efficiency_scale, TRUE);
    gtk_widget_set_sensitive(widgets->drive_mode_dropdown, TRUE);
    gtk_widget_set_sensitive(widgets->accel_spin, FALSE);
}

static void reset_simulation(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;    
    sim_data.vehicle_speed = 0;
    sim_data.motor_rpm = 0;
    sim_data.motor_torque = 0;
    sim_data.distance = 0;
    sim_data.energy_consumed = 0;
    sim_data.soc = 100;
    sim_data.battery_temp = 25.0;
    sim_data.energy_efficiency = 0;
    last_time = 0;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f km/h", sim_data.vehicle_speed);
    gtk_label_set_text(GTK_LABEL(widgets->speed_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.1f %%", sim_data.soc);
    gtk_label_set_text(GTK_LABEL(widgets->soc_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.2f km", sim_data.distance);
    gtk_label_set_text(GTK_LABEL(widgets->distance_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.2f kWh", sim_data.energy_consumed);
    gtk_label_set_text(GTK_LABEL(widgets->energy_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.1f Nm", sim_data.motor_torque);
    gtk_label_set_text(GTK_LABEL(widgets->torque_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.0f RPM", sim_data.motor_rpm);
    gtk_label_set_text(GTK_LABEL(widgets->rpm_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.1f °C", sim_data.battery_temp);
    gtk_label_set_text(GTK_LABEL(widgets->temp_label), buffer);
    snprintf(buffer, sizeof(buffer), "%.0f Wh/km", sim_data.energy_efficiency);
    gtk_label_set_text(GTK_LABEL(widgets->efficiency_label), buffer);
    gtk_widget_queue_draw(widgets->drawing_area);
}

static void cleanup(GtkWidget *widget, gpointer data) {
    g_free(data);
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_new0(AppWidgets, 1);
    if (!widgets) {
        g_error("Failed to allocate AppWidgets");
        return;
    }
    widgets->window = gtk_application_window_new(app);
    if (!widgets->window) {
        g_error("Failed to create main window");
        g_free(widgets);
        return;
    }
    gtk_window_set_title(GTK_WINDOW(widgets->window), "EV Powertrain Simulation");
    gtk_window_set_default_size(GTK_WINDOW(widgets->window), 400, 700);
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_window_set_child(GTK_WINDOW(widgets->window), main_box);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 10);
    gtk_widget_set_margin_bottom(main_box, 10);
    GtkWidget *control_frame = gtk_frame_new("Controls");
    gtk_box_append(GTK_BOX(main_box), control_frame);
    GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_frame_set_child(GTK_FRAME(control_frame), control_box);
    gtk_widget_set_margin_start(control_box, 10);
    gtk_widget_set_margin_end(control_box, 10);
    gtk_widget_set_margin_top(control_box, 10);
    gtk_widget_set_margin_bottom(control_box, 10);
    GtkWidget *voltage_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *battery_voltage_label = gtk_label_new("Battery Voltage (V):");
    gtk_label_set_xalign(GTK_LABEL(battery_voltage_label), 0);
    widgets->battery_voltage_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(widgets->battery_voltage_entry), "400");
    gtk_widget_set_size_request(widgets->battery_voltage_entry, 100, 40);
    gtk_box_append(GTK_BOX(voltage_box), battery_voltage_label);
    gtk_box_append(GTK_BOX(voltage_box), widgets->battery_voltage_entry);
    gtk_box_append(GTK_BOX(control_box), voltage_box);
    GtkWidget *capacity_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *battery_capacity_label = gtk_label_new("Battery Capacity (kWh):");
    gtk_label_set_xalign(GTK_LABEL(battery_capacity_label), 0);
    widgets->battery_capacity_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(widgets->battery_capacity_entry), "60");
    gtk_widget_set_size_request(widgets->battery_capacity_entry, 100, 40);
    gtk_box_append(GTK_BOX(capacity_box), battery_capacity_label);
    gtk_box_append(GTK_BOX(capacity_box), widgets->battery_capacity_entry);
    gtk_box_append(GTK_BOX(control_box), capacity_box);
    GtkWidget *power_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *motor_power_label = gtk_label_new("Motor Power (kW):");
    gtk_label_set_xalign(GTK_LABEL(motor_power_label), 0);
    widgets->motor_power_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(widgets->motor_power_entry), "150");
    gtk_widget_set_size_request(widgets->motor_power_entry, 100, 40);
    gtk_box_append(GTK_BOX(power_box), motor_power_label);
    gtk_box_append(GTK_BOX(power_box), widgets->motor_power_entry);
    gtk_box_append(GTK_BOX(control_box), power_box);
    GtkWidget *regen_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *regen_braking_label = gtk_label_new("Regen Braking:");
    gtk_label_set_xalign(GTK_LABEL(regen_braking_label), 0);
    widgets->regen_braking_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(widgets->regen_braking_switch), TRUE);
    gtk_box_append(GTK_BOX(regen_box), regen_braking_label);
    gtk_box_append(GTK_BOX(regen_box), widgets->regen_braking_switch);
    gtk_box_append(GTK_BOX(control_box), regen_box);
    GtkWidget *regen_eff_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *regen_eff_label = gtk_label_new("Regen Efficiency (%):");
    gtk_label_set_xalign(GTK_LABEL(regen_eff_label), 0);
    widgets->regen_efficiency_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(widgets->regen_efficiency_scale), 50);
    gtk_widget_set_size_request(widgets->regen_efficiency_scale, 100, 40);
    gtk_box_append(GTK_BOX(regen_eff_box), regen_eff_label);
    gtk_box_append(GTK_BOX(regen_eff_box), widgets->regen_efficiency_scale);
    gtk_box_append(GTK_BOX(control_box), regen_eff_box);
    GtkWidget *drive_mode_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *drive_mode_label = gtk_label_new("Drive Mode:");
    gtk_label_set_xalign(GTK_LABEL(drive_mode_label), 0);
    GtkStringList *drive_modes = gtk_string_list_new(NULL);
    gtk_string_list_append(drive_modes, "Eco");
    gtk_string_list_append(drive_modes, "Normal");
    gtk_string_list_append(drive_modes, "Sport");
    widgets->drive_mode_dropdown = gtk_drop_down_new(G_LIST_MODEL(drive_modes), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(widgets->drive_mode_dropdown), DRIVE_MODE_NORMAL);
    gtk_widget_set_size_request(widgets->drive_mode_dropdown, 100, 40);
    gtk_box_append(GTK_BOX(drive_mode_box), drive_mode_label);
    gtk_box_append(GTK_BOX(drive_mode_box), widgets->drive_mode_dropdown);
    gtk_box_append(GTK_BOX(control_box), drive_mode_box);
    GtkWidget *accel_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *accel_label = gtk_label_new("Acceleration (m/s²):");
    gtk_label_set_xalign(GTK_LABEL(accel_label), 0);
    widgets->accel_spin = gtk_spin_button_new_with_range(-1.5, 1.5, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->accel_spin), 0.0);
    gtk_widget_set_size_request(widgets->accel_spin, 100, 40);
    gtk_widget_set_sensitive(widgets->accel_spin, FALSE);
    gtk_box_append(GTK_BOX(accel_box), accel_label);
    gtk_box_append(GTK_BOX(accel_box), widgets->accel_spin);
    gtk_box_append(GTK_BOX(control_box), accel_box);
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    widgets->start_button = gtk_button_new_with_label("Start");
    gtk_widget_set_size_request(widgets->start_button, 100, 50);
    g_signal_connect(widgets->start_button, "clicked", G_CALLBACK(start_simulation), widgets);
    widgets->stop_button = gtk_button_new_with_label("Stop");
    gtk_widget_set_size_request(widgets->stop_button, 100, 50);
    g_signal_connect(widgets->stop_button, "clicked", G_CALLBACK(stop_simulation), widgets);
    gtk_widget_set_sensitive(widgets->stop_button, FALSE);
    widgets->reset_button = gtk_button_new_with_label("Reset");
    gtk_widget_set_size_request(widgets->reset_button, 100, 50);
    g_signal_connect(widgets->reset_button, "clicked", G_CALLBACK(reset_simulation), widgets);
    gtk_widget_set_sensitive(widgets->reset_button, FALSE);
    gtk_box_append(GTK_BOX(button_box), widgets->start_button);
    gtk_box_append(GTK_BOX(button_box), widgets->stop_button);
    gtk_box_append(GTK_BOX(button_box), widgets->reset_button);
    gtk_box_append(GTK_BOX(control_box), button_box);
    GtkWidget *status_frame = gtk_frame_new("Status");
    gtk_box_append(GTK_BOX(main_box), status_frame);
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_frame_set_child(GTK_FRAME(status_frame), status_box);
    gtk_widget_set_margin_start(status_box, 10);
    gtk_widget_set_margin_end(status_box, 10);
    gtk_widget_set_margin_top(status_box, 10);
    gtk_widget_set_margin_bottom(status_box, 10);
    GtkWidget *speed_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *speed_text_label = gtk_label_new("Speed:");
    gtk_label_set_xalign(GTK_LABEL(speed_text_label), 0);
    widgets->speed_label = gtk_label_new("0 km/h");
    gtk_box_append(GTK_BOX(speed_box), speed_text_label);
    gtk_box_append(GTK_BOX(speed_box), widgets->speed_label);
    gtk_box_append(GTK_BOX(status_box), speed_box);
    GtkWidget *soc_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *soc_text_label = gtk_label_new("State of Charge:");
    gtk_label_set_xalign(GTK_LABEL(soc_text_label), 0);
    widgets->soc_label = gtk_label_new("100 %");
    gtk_box_append(GTK_BOX(soc_box), soc_text_label);
    gtk_box_append(GTK_BOX(soc_box), widgets->soc_label);
    gtk_box_append(GTK_BOX(status_box), soc_box);
    GtkWidget *distance_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *distance_text_label = gtk_label_new("Distance:");
    gtk_label_set_xalign(GTK_LABEL(distance_text_label), 0);
    widgets->distance_label = gtk_label_new("0 km");
    gtk_box_append(GTK_BOX(distance_box), distance_text_label);
    gtk_box_append(GTK_BOX(distance_box), widgets->distance_label);
    gtk_box_append(GTK_BOX(status_box), distance_box);
    GtkWidget *energy_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *energy_text_label = gtk_label_new("Energy Consumed:");
    gtk_label_set_xalign(GTK_LABEL(energy_text_label), 0);
    widgets->energy_label = gtk_label_new("0 kWh");
    gtk_box_append(GTK_BOX(energy_box), energy_text_label);
    gtk_box_append(GTK_BOX(energy_box), widgets->energy_label);
    gtk_box_append(GTK_BOX(status_box), energy_box);
    GtkWidget *torque_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *torque_text_label = gtk_label_new("Motor Torque:");
    gtk_label_set_xalign(GTK_LABEL(torque_text_label), 0);
    widgets->torque_label = gtk_label_new("0 Nm");
    gtk_box_append(GTK_BOX(torque_box), torque_text_label);
    gtk_box_append(GTK_BOX(torque_box), widgets->torque_label);
    gtk_box_append(GTK_BOX(status_box), torque_box);
    GtkWidget *rpm_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *rpm_text_label = gtk_label_new("Motor RPM:");
    gtk_label_set_xalign(GTK_LABEL(rpm_text_label), 0);
    widgets->rpm_label = gtk_label_new("0 RPM");
    gtk_box_append(GTK_BOX(rpm_box), rpm_text_label);
    gtk_box_append(GTK_BOX(rpm_box), widgets->rpm_label);
    gtk_box_append(GTK_BOX(status_box), rpm_box);
    GtkWidget *temp_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *temp_text_label = gtk_label_new("Battery Temp:");
    gtk_label_set_xalign(GTK_LABEL(temp_text_label), 0);
    widgets->temp_label = gtk_label_new("25.0 °C");
    gtk_box_append(GTK_BOX(temp_box), temp_text_label);
    gtk_box_append(GTK_BOX(temp_box), widgets->temp_label);
    gtk_box_append(GTK_BOX(status_box), temp_box);
    GtkWidget *efficiency_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *efficiency_text_label = gtk_label_new("Efficiency:");
    gtk_label_set_xalign(GTK_LABEL(efficiency_text_label), 0);
    widgets->efficiency_label = gtk_label_new("0 Wh/km");
    gtk_box_append(GTK_BOX(efficiency_box), efficiency_text_label);
    gtk_box_append(GTK_BOX(efficiency_box), widgets->efficiency_label);
    gtk_box_append(GTK_BOX(status_box), efficiency_box);
    GtkWidget *waveform_frame = gtk_frame_new("Waveforms");
    gtk_box_append(GTK_BOX(main_box), waveform_frame);
    widgets->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(widgets->drawing_area, 380, 300);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(widgets->drawing_area), draw_waveforms, NULL, NULL);
    gtk_frame_set_child(GTK_FRAME(waveform_frame), widgets->drawing_area);
    for (int i = 0; i < WAVE_POINTS; i++) {
        voltage_wave[i] = 0;
        current_wave[i] = 0;
        speed_wave[i] = 0;
        temp_wave[i] = 0;
    }
    g_timeout_add(200, update_simulation, widgets);
    g_signal_connect(widgets->window, "destroy", G_CALLBACK(cleanup), widgets);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, 
        "label, button, entry, spinbutton, scale, dropdown { font-size: 16px; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    gtk_widget_set_visible(widgets->window, TRUE);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.example.evsimulator", G_APPLICATION_DEFAULT_FLAGS);
    if (!app) {
        fprintf(stderr, "Failed to create GTK application\n");
        return 1;
    }
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
