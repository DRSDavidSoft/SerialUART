/* Dialog geometry for the shared table cells, rules and inline editor. */
typedef struct {
    unsigned x, y, width, height, columns, rows, visible, label, value, stride;
} PanelDialog;

static PanelDialog panel_dialog_layout(ps_session *s) {
    const ps_control_page *page = &s->panel->pages[s->tab];
    PanelDialog box;
    unsigned i, space = s->io.columns - 2;
    box.label = 1;
    box.value = 8;
    for (i = 0; i < page->count; ++i) {
        const ps_control *control = &page->controls[i];
        ps_control_value value;
        char display[80];
        unsigned width_ = cells(control->label, length(control->label), s->io.flags);
        if (width_ > box.label) box.label = width_;
        control_read(s, control, &value);
        control_value_text(s, control, &value, display, sizeof display);
        width_ = cells(display, length(display), s->io.flags);
        if (width_ > box.value) box.value = width_;
        if (control->kind == PS_CONTROL_CHOICE) {
            unsigned choice;
            for (choice = 0; choice < control->choice_count; ++choice) {
                const char *label = control->choices[choice];
                width_ = cells(label, length(label), s->io.flags);
                if (width_ > box.value) box.value = width_;
            }
        }
    }
    box.label = min(box.label, 36);
    box.value = min(box.value, 32);
    box.stride = box.label + box.value + 6;
    box.columns = min(s->panel_dialog_columns, min(page->count ? page->count : 1, space / box.stride));
    if (!box.columns) {
        box.columns = 1;
        box.label = min(box.label, space > 7 ? (space - 6) / 2 : 1);
        box.value = space > box.label + 6 ? space - box.label - 6 : 1;
        box.stride = box.label + box.value + 6;
    }
    box.width = box.columns * box.stride + 1;
    if (box.columns == 1 && box.width < 38 && box.width < s->io.columns - 1) {
        unsigned extra = min(38, s->io.columns - 1) - box.width;
        box.value += extra;
        box.stride += extra;
        box.width += extra;
    }
    box.rows = (page->count + box.columns - 1) / box.columns;
    box.visible = s->io.rows > 6 ? min(box.rows, s->io.rows - 6) : 0;
    box.height = box.visible + 6;
    box.x = box.width < s->io.columns ? (s->io.columns - box.width) / 2 + 1 : 1;
    box.y = box.height < s->io.rows ? (s->io.rows - box.height) / 2 + 1 : 1;
    return box;
}

static void panel_dialog_rule(ps_session *s, const Grid *grid, unsigned y, unsigned x,
                              unsigned kind, int joined) {
    at(s, y, x);
    surface(s, 0);
    grid_border(s, grid, kind, joined);
}

static void panel_dialog_draw(ps_session *s) {
    const ps_control_page *page = &s->panel->pages[s->tab];
    PanelDialog box = panel_dialog_layout(s);
    Grid grid = {0}, pair = {0}, footer_grid = {0};
    unsigned row, col, focus = s->selected / box.columns;
    unsigned footer_y = box.y + box.visible + 3;
    const char *footer = (s->io.flags & PS_UNICODE) ? "←↑↓→ Move  Enter Edit  Esc Close"
                                                  : "Tab Move  Enter Edit  Esc Close";
    s->dirty = 0;
    s->last_paint = s->now;
    s->mouse_edit_width = 0;
    if (focus < s->scroll) s->scroll = focus;
    if (focus >= s->scroll + box.visible && box.visible) s->scroll = focus - box.visible + 1;
    s->scroll = min(s->scroll, box.rows - box.visible);

    /* Erase in the theme's base color before drawing either layer. */
    reset(s);
    s->canvas_kind = 0;
    if (s->menu_screen && menu_framed(s)) menu_draw(s);
    else ansi(s, "\033[2J");
    frame_begin(s);
    ansi(s, "\033[?25l");
    if (s->io.columns < 12 || s->io.rows < 7) {
        at(s, 1, 1);
        field(s, "Resize or Esc", s->io.columns - 1);
        frame_end(s);
        return;
    }

    pair.count = 2;
    pair.widths[0] = box.label;
    pair.widths[1] = box.value;
    grid.count = box.columns * 2;
    for (col = 0; col < grid.count; ++col)
        grid.widths[col] = pair.widths[col % 2];
    footer_grid.count = 1;
    footer_grid.widths[0] = box.width - 4;

    panel_dialog_rule(s, &grid, box.y, box.x, 0, 1);
    at(s, box.y, box.x + 2);
    surface(s, 0);
    ink(s, PS_INK_TEXT);
    out(s, " ");
    field(s, s->panel->title, min(box.width - 6,
          cells(s->panel->title, length(s->panel->title), s->io.flags)));
    out(s, " ");
    reset(s);

    at(s, box.y + 1, box.x);
    surface(s, 0);
    grid_vertical(s);
    out(s, " ");
    for (col = 0; col < grid.count; ++col) {
        ink(s, PS_INK_HEADING);
        field(s, col % 2 ? "Value" : "Control", grid.widths[col]);
        if (col + 1 < grid.count) grid_divide(s);
    }
    out(s, " ");
    grid_vertical(s);
    reset(s);
    panel_dialog_rule(s, &grid, box.y + 2, box.x, 1, 1);

    for (row = 0; row < box.visible; ++row) {
        unsigned y = box.y + row + 3;
        at(s, y, box.x);
        surface(s, 0);
        grid_vertical(s);
        out(s, " ");
        for (col = 0; col < box.columns; ++col) {
            unsigned index = (s->scroll + row) * box.columns + col;
            if (index < page->count) {
                if (index == s->selected) {
                    if (s->io.flags & PS_COLOR) surface(s, 2);
                    else ansi(s, "\033[7m");
                }
                control_row(s, index, &pair);
                reset(s);
                surface(s, 0);
                if (index == s->selected && s->editing && !s->dialog) {
                    unsigned start = 0;
                    while (start < s->editpos &&
                           cells(s->edit + start, s->editpos - start, s->io.flags) >= box.value)
                        start = next(s->edit, start);
                    s->mouse_edit_x = box.x + 2 + col * box.stride + box.label + 3;
                    s->mouse_edit_y = y;
                    s->mouse_edit_start = start;
                    s->mouse_edit_width = box.value;
                }
            } else {
                spaces(s, box.label);
                grid_divide(s);
                spaces(s, box.value);
            }
            if (col + 1 < box.columns) grid_divide(s);
        }
        out(s, " ");
        grid_vertical(s);
        reset(s);
    }
    panel_dialog_rule(s, &grid, footer_y, box.x, 3, 1);
    if (s->editing) footer = "Enter Apply  Esc Cancel";
    if (s->notice[0]) footer = s->notice;
    at(s, footer_y + 1, box.x);
    surface(s, 0);
    grid_vertical(s);
    out(s, " ");
    ink(s, PS_INK_MUTED);
    field(s, footer, box.width - 4);
    out(s, " ");
    grid_vertical(s);
    reset(s);
    panel_dialog_rule(s, &footer_grid, footer_y + 2, box.x, 2, 0);

    if (s->mouse_edit_width) {
        at(s, s->mouse_edit_y, s->mouse_edit_x +
           cells(s->edit + s->mouse_edit_start, s->editpos - s->mouse_edit_start, s->io.flags));
        ansi(s, "\033[?25h");
    } else if (s->editing && s->dialog && s->io.rows >= 14 && s->io.columns >= 42) {
        control_overlay(s, &pair);
    }
    frame_end(s);
}

static void panel_dialog_key(ps_session *s, unsigned key) {
    const ps_control_page *page = &s->panel->pages[s->tab];
    const ps_control *control = control_selected(s);
    unsigned columns = panel_dialog_layout(s).columns;
    s->dirty = 1;
    if (s->editing) { control_key(s, key); return; }
    if (key == 27 || key == 3) { table_close(s); return; }
    if (!page->count || s->io.columns < 12 || s->io.rows < 7) return;
    s->notice[0] = 0;
    if (key == PS_KEY_UP && s->selected >= columns) s->selected -= columns;
    else if (key == PS_KEY_DOWN && s->selected + columns < page->count) s->selected += columns;
    else if (key == PS_KEY_RIGHT || key == 9) s->selected = (s->selected + 1) % page->count;
    else if (key == PS_KEY_LEFT || key == PS_KEY_BACKTAB)
        s->selected = (s->selected + page->count - 1) % page->count;
    else if (key == PS_KEY_HOME) s->selected = 0;
    else if (key == PS_KEY_END) s->selected = page->count - 1;
    else if ((key == 13 || key == ' ') && control) {
        if (control->kind == PS_CONTROL_TOGGLE || control->kind == PS_CONTROL_CHOICE) {
            ps_control_value value;
            if (!control_writable(s, control, &value)) return;
            value.number = control->kind == PS_CONTROL_TOGGLE ? !value.number :
                           (value.number + 1) % control->choice_count;
            control_commit(s, control, &value, value.revision);
        } else if (control->kind != PS_CONTROL_ACTION || (s->io.rows >= 14 && s->io.columns >= 42))
            control_begin(s, control, 0);
        else notice(s, "Resize to confirm action");
    }
}

#if PS_MOUSE
static void panel_dialog_mouse(ps_session *s, const ps_mouse_event *event) {
    PanelDialog box = panel_dialog_layout(s);
    unsigned column, index;
    if (s->io.columns < 12 || s->io.rows < 7 || event->release || (event->button & 60u)) return;
    if (s->editing) {
        if (event->button == 2) panel_dialog_key(s, 27);
        return;
    }
    if (event->button == 64 || event->button == 65) {
        panel_dialog_key(s, event->button == 64 ? PS_KEY_UP : PS_KEY_DOWN);
        return;
    }
    if (event->button || event->column < box.x + 2 || event->row < box.y + 3 ||
        event->row >= box.y + 3 + box.visible) return;
    column = (event->column - box.x - 2) / box.stride;
    if (column >= box.columns || event->column >= box.x + box.width - 1) return;
    index = (s->scroll + event->row - box.y - 3) * box.columns + column;
    if (index >= s->panel->pages[s->tab].count) return;
    s->selected = index;
    s->notice[0] = 0;
    s->dirty = 1;
}
#endif

PS_API int ps_panel_open_dialog(ps_session *s, unsigned page, unsigned columns) {
    if (!s || !s->panel || page >= s->panel->count || columns < 1 || columns > 4 ||
        s->board || s->document_open || ps_form_active(s) || ps_hex_editor_active(s) ||
        s->history_dialog || !(s->io.flags & PS_INTERACTIVE))
        return 0;
    ps_file_picker_close(s);
    if (!s->menu_screen) {
        ansi(s, "\033[?1049h");
        if (PS_MOUSE) ansi(s, "\033[?1002h\033[?1006h");
    }
    s->board = 1;
    s->tab = page;
    s->selected = s->scroll = 0;
    s->panel_dialog_columns = columns;
    s->control_view = 1;
    s->control_header = s->editing = s->dialog = 0;
    s->notice[0] = 0;
    panel_dialog_draw(s);
    return 1;
}
