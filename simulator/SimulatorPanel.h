// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LED panel of the simulator, as a module of the chosen wiring lights it.
 *
 * Reads the model's LEDs rather than matrix_buffer_, so a sketch with the wrong orientation for its
 * module shows it wrong here too - which is the point. Knows nothing of where the panel is drawn.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORPANEL_H_
#define MAXMATRIX_SIMULATOR_SIMULATORPANEL_H_

#include <string>
#include "Arduino.h"
#include "MaxMatrixConfig.h"

/* SimulatorPanel Parameter */
#define SIMULATOR_PANEL_DEFAULT_WIRING                              "FC16"
/* about 30 pictures a second; neither a terminal nor a page keeps up with every frame */
#define SIMULATOR_PANEL_REDRAW_INTERVAL                             33u

/*!
 * \brief The LED panel of the simulator, as a module of the chosen wiring lights it.
 *
 * It also keeps when it was drawn last, as whether it has to be drawn again depends on what
 * changed on it: a frame, the wiring or the mounting.
 */
class SimulatorPanel
{
  private:
    std::string wiring_name_;
    Max7219Wiring const* wiring_;
    bool upside_down_;
    int32_t drawn_frames_;
    uint32_t last_draw_;
    bool draw_requested_;

    static Max7219Wiring const* FindWiring(std::string const& name);

  public:
    /*! \brief Makes a panel of the default wiring, mounted upright, that has never been drawn. */
    SimulatorPanel();

    /*! \brief Returns the name of the chosen wiring. */
    std::string const& GetWiringName() const { return wiring_name_; }

    /*! \brief Returns whether the board is mounted upside down. */
    bool IsUpsideDown() const { return upside_down_; }

    /*!
     * \brief Returns the panel as text, the LEDs row after row, as the chosen module lights them.
     *
     * \param[in] lit What stands for a lit LED.
     * \param[in] dark What stands for a dark LED.
     * \param[in] end_of_row What follows every row.
     * \return The panel.
     */
    std::string GetText(char const* lit, char const* dark, char const* end_of_row) const;

    /*!
     * \brief Returns the brightness of a module.
     *
     * The chip that lights the module at this place depends on the mounting.
     *
     * \param[in] module The module's place, counted from the left as the viewer sees the panel.
     * \return The intensity register of its chip.
     */
    uint8_t GetIntensity(uint8_t module) const;

    /*!
     * \brief Chooses the wiring of the modules.
     *
     * It takes the four names MD_MAX72XX has, `FC16`, `GENERIC`, `PAROLA` and `ICSTATION`, and
     * `DR0CR0RR0` ... `DR1CR1RR1`.
     *
     * \param[in] name The wiring's name.
     * \return True once the wiring is chosen, or false if no wiring has this name; the panel then
     *         keeps the one it has.
     */
    bool SetWiring(std::string const& name);

    /*!
     * \brief Chooses how the board is mounted.
     *
     * The panel is turned, so that the next draw shows it as a board mounted this way lights it.
     *
     * \param[in] upside_down True if the board is mounted upside down.
     */
    void SetUpsideDown(bool upside_down);

    /*! \brief Asks for a draw, so that IsDrawDue() says so even without a new frame. */
    void RequestDraw() { draw_requested_ = true; }

    /*!
     * \brief Returns whether the panel has to be drawn.
     *
     * It has when a frame arrived or a draw was requested since the last draw, but no more often
     * than every `SIMULATOR_PANEL_REDRAW_INTERVAL` milliseconds.
     *
     * \param[in] now The time, as `millis()` gives it.
     * \return True if it is time to draw.
     */
    bool IsDrawDue(uint32_t now) const;

    /*!
     * \brief Notes that the panel was drawn.
     *
     * It keeps the frame count and the time, so that IsDrawDue() waits for the next change.
     *
     * \param[in] now The time, as `millis()` gives it.
     */
    void SetDrawn(uint32_t now);
};

#endif
