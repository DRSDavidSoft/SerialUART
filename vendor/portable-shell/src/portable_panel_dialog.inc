/* A page dialog shares the ordinary panel's validation and inline editor. */
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
    box.stride = box.label + box.value + 5;
    box.columns = min(s->panel_dialog_columns, min(page->count ? page->count : 1, space / box.stride));
    if (!box.columns) {
        box.columns = 1;
        box.label = min(box.label, (space - 5) / 2);
        box.value = space - 5 - box.label;
        box.stride = space;
    }
    box.width = box.columns * box.stride + 1;
    box.width = min(s->io.columns - 1, box.width < 38 ? 38 : box.width);
    box.rows = (page->count + box.columns - 1) / box.columns;
    box.visible = min(box.rows, s->io.rows > 5 ? s->io.rows - 5 : s->io.rows - 3);
    box.height = box.visible + 3;
    box.x = (s->io.columns - box.width) / 2 + 1;
    box.y = (s->io.rows - box.height) / 2 + 1;
    return box;
}

static void panel_dialog_draw(ps_session *s) {
    const ps_control_page *page = &s->panel->pages[s->tab];
    PanelDialog box = panel_dialog_layout(s);
    unsigned row, col, focus = s->selected / box.columns;
    const char *footer = (s->io.flags & PS_UNICODE) ? "←↑↓→ Move  Enter Edit  Esc Close"
                                                  : "Tab Move  Enter Edit  Esc Close";
    s->dirty = 0;
    s->last_paint = s->now;
    s->mouse_edit_width = 0;
    if (focus < s->scroll) s->scroll = focus;
    if (focus >= s->scroll + box.visible && box.visible) s->scroll = focus - box.visible + 1;
    s->scroll = min(s->scroll, box.rows - box.visible);

    frame_begin(s);
    s->canvas_kind = 0;
    if (s->menu_screen && menu_framed(s)) menu_draw(s);
    else {
        reset(s);
        ansi(s, "\033[2J");
    }
    view_rule_color(s, box.y, box.x, box.width, 0, "\033[36m");
    at(s, box.y, box.x + 1);
    surface(s, 2);
    ink(s, PS_INK_HEADING);
    field(s, s->panel->title, min(box.width - 2,
          cells(s->panel->title, length(s->panel->title), s->io.flags)));

    for (row = 0; row <= box.visible; ++row) {
        at(s, box.y + row + 1, box.x);
        surface(s, 2);
        ink(s, PS_INK_BORDER);
        out(s, (s->io.flags & PS_UNICODE) ? "│" : "|");
        spaces(s, box.width - 2);
        out(s, (s->io.flags & PS_UNICODE) ? "│" : "|");
    }
    view_rule_color(s, box.y + box.height - 1, box.x, box.width, 1, "\033[36m");

    for (row = 0; row < box.visible; ++row) {
        for (col = 0; col < box.columns; ++col) {
            unsigned index = (s->scroll + row) * box.columns + col;
            unsigned y = box.y + row + 1;
            unsigned x = box.x + 2 + col * box.stride;
            const ps_control *control;
            ps_control_value value;
            char display[80];
            if (index >= page->count) break;
            control = &page->controls[index];
            control_read(s, control, &value);
            control_value_text(s, control, &value, display, sizeof display);
            at(s, y, x);
            surface(s, 2);
            if (index == s->selected) view_selection(s);
            else ink(s, value.flags & PS_CONTROL_LOCKED ? PS_INK_INACTIVE : PS_INK_TEXT);
            grid_text(s, control->label, box.label);
            out(s, "  ");
            if (index != s->selected)
                ink(s, value.flags & PS_CONTROL_LOCKED ? PS_INK_INACTIVE :
                       control->kind == PS_CONTROL_LEVEL ? PS_INK_NUMBER : PS_INK_ACTION);
            if (index == s->selected && s->editing && !s->dialog) {
                unsigned start = 0;
                while (start < s->editpos &&
                       cells(s->edit + start, s->editpos - start, s->io.flags) >= box.value)
                    start = next(s->edit, start);
                grid_text(s, s->edit + start, box.value);
                s->mouse_edit_x = x + box.label + 2;
                s->mouse_edit_y = y;
                s->mouse_edit_start = start;
                s->mouse_edit_width = box.value;
            } else grid_text(s, display, box.value);
            reset(s);
        }
    }
    if (s->editing) footer = "Enter Apply  Esc Cancel";
    if (s->notice[0]) footer = s->notice;
    at(s, box.y + box.visible + 1, box.x + 1);
    surface(s, 2);
    ink(s, PS_INK_MUTED);
    field(s, footer, box.width - 2);
    reset(s);
    if (s->mouse_edit_width) {
        at(s, s->mouse_edit_y, s->mouse_edit_x +
           cells(s->edit + s->mouse_edit_start, s->editpos - s->mouse_edit_start, s->io.flags));
        ansi(s, "\033[?25h");
    } else if (s->editing && s->dialog && s->io.rows >= 14 && s->io.columns >= 42) {
        control_overlay(s, 0); /* The action confirmation does not use table geometry. */
    } else ansi(s, "\033[?25l");
    frame_end(s);
}

static void panel_dialog_key(ps_session *s, unsigned key) {
    const ps_control_page *page = &s->panel->pages[s->tab];
    const ps_control *control = control_selected(s);
    unsigned columns = panel_dialog_layout(s).columns;
    s->dirty = 1;
    if (s->editing) { control_key(s, key); return; }
    if (key == 27 || key == 3) { table_close(s); return; }
    if (!page->count) return;
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
    if (event->release || (event->button & 60u)) return;
    if (s->editing) {
        if (event->button == 2) panel_dialog_key(s, 27);
        return;
    }
    if (event->button == 64 || event->button == 65) {
        panel_dialog_key(s, event->button == 64 ? PS_KEY_UP : PS_KEY_DOWN);
        return;
    }
    if (event->button || event->column < box.x + 2 || event->row <= box.y ||
        event->row > box.y + box.visible) return;
    column = (event->column - box.x - 2) / box.stride;
    if (column >= box.columns || event->column >= box.x + box.width - 1) return;
    index = (s->scroll + event->row - box.y - 1) * box.columns + column;
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
