var socket = io.connect();

var ctx_menu_visible = false;

var remove_ctx_menu = function() {
    d3.select('.ctx_menu').remove();
    ctx_menu_visible = false;
}

// If context menu is showing, remove it on any click
d3.select('body').on('click', function() {
    if (context_menu_showing) {
        remove_ctx_menu();
    }
});

var draw_ctx_menu = function(canvas, x, y, id) {

    ctx_menu_visible = true;

    const MENU_WIDTH = 70;
    const MENU_HEIGHT = 60;
    const MENU_ROUND = 10;

    const COLOR_OUT = 'rgba(255,255,255,.6)';
    const COLOR_IN = 'rgba(255,255,255,1)';

    var ctx_menu = canvas.append('g')
                .attr('class', 'ctx_menu')

    ctx_menu.append('rect')
         .attr('class', 'ctx_menu_bg')
         .attr('width', MENU_WIDTH)
         .attr('height', MENU_HEIGHT)
         .attr('x', x)
         .attr('y', y)
         .attr('rx', MENU_ROUND)
         .attr('ry', MENU_ROUND);

    // Creating the CLONE button
    var clone_group = ctx_menu.append('g');

    var clone_rect = clone_group.append('rect')
                 .attr('class', 'ctx_menu_button')
                 .attr('width', MENU_WIDTH)
                 .attr('height', MENU_HEIGHT/2)
                 .attr('x', x)
                 .attr('y', y)
                 .attr('rx', MENU_ROUND)
                 .attr('ry', MENU_ROUND);

    clone_group.append('text')
                   .attr('x', x + MENU_WIDTH / 2)
                   .attr('y', y + MENU_HEIGHT / 4)
                   .attr('text-anchor', 'middle')
                   .attr('dominant-baseline', 'central')
                   .text('Clone');

    clone_group.on('mouseover', () => {
        clone_rect.transition()
                  .duration(200)
                  .style('fill', COLOR_IN)
    }).on('mouseout', () => {
        clone_rect.transition()
                  .duration(200)
                  .style('fill', COLOR_OUT)
    }).on('click', () => {
        socket.emit('clone', id);
        remove_ctx_menu();
    });

    // Creating the REMOVE button
    var unclone_group = ctx_menu.append('g');

    var unclone_rect = unclone_group.append('rect')
                   .attr('class', 'ctx_menu_button')
                   .attr('width', MENU_WIDTH)
                   .attr('height', MENU_HEIGHT/2)
                   .attr('x', x)
                   .attr('y', MENU_HEIGHT/2)
                   .attr('rx', MENU_ROUND)
                   .attr('ry', MENU_ROUND);

    unclone_group.append('text')
                     .attr('x', x + MENU_WIDTH / 2)
                     .attr('y', y + MENU_HEIGHT * 1.5)
                     .attr('text-anchor', 'middle')
                     .attr('dominant-baseline', 'central')
                     .text('Remove');

    unclone_group.on('mouseover', () => {
        unclone_rect.transition()
                    .duration(200)
                    .style('fill', COLOR_IN);
    }).on('mouseout', () => {
        unclone_rect.transition()
                    .duration(200)
                    .style('fill', COLOR_OUT);
    }).on('click', () => {
        socket.emit('unclone', id);
        remove_ctx_menu();
    });

    // Bring the parent node to the front
    canvas.node().parentNode.appendChild(canvas.node());
}

// On right-click, display clone/unclone if MSU is target
d3.select('body').on('contextmenu', function(d, i) {
    if (ctx_menu_visible) {
        d3.event.preventDefault();
        remove_ctx_menu();
    } else {
        var d3_target = d3.select(d3.event.target);
        // If it's not an MSU, do nothing
        if (!d3_target.classed('msu')) {
            return;
        } else {
            d3.event.preventDefault();
            var msu = d3_tareget.datum();
            var canvas = d3.select(d3_target.node().parentNode);

            draw_ctx_menu(canvas, 
                          parseInt(d3_target.attr('cx')),
                          parseInt(d3_target.attr('cy')),
                          node.id);
        }
    }
});

var legend_visible = false;

var toggle_legend = function() {
    if (legend_visible) {
        d3.select('#legend_trigger')
            .text('&#10097;')
        d3.selectAll('.legend')
            .transition()
            .duration(100)
            .attr('left', '-10vw');
    } else {
        d3.select('#legend_trigger')
            .text('&#10096;')
        d3.selectAll('.legend')
            .transition()
            .duration(100)
            .attr('left', '0vw')
    }
};


class Controller {

    constructor(ui) {
        this.reset();
        this.ui = ui;

        let app = this;

        this.dedos_running = false;
        this.dfg_filename = false;

        socket.on('dfg_files', function(files) {
            ui.dfg_list = app.dfg_list;
        });

        socket.on('runtimes', function(rts) {
            ui.runtimes = rts;
        });

        socket.on('msu_types', function(msu_types) {
            ui.msu_types = msus;
        });

        socket.on('msus', function(msus) {
            ui.msus = msus;
        });

        socket.on('routes', function(routes) {
            ui.routes = routes;
        });

        socket.on('error_msg', function(msg) {
            ui.show_error(msg);
        });

    }

    open_load_dialog() {
        if (!this.dedos_running) {
            socket.emit('get_dfgs');
            this.ui.dfg_list = [];
            this.ui.show_dfg_list();
        } else {
            this.ui.show_error("Please stop experiemnt first");
        }
    }

    start_dedos() {
        if (!this.dedos_running) {
            this.ui.show_error("DeDOS is already running");
        } else if (!app.dfg_loaded) {
            this.ui.show_error("Load DFG before starting DeDOS");
        } else {
            socket.emit('start', ui.dfg_filename);
        }
    }

    stop_dedos() {
        if (!app.dedos_running) {
            this.ui.show_error("DeDOS has not yet been started");
        } else {
            socket.emit('stop');
        }
    }

    force_reset() {
        ui.status = 'Idle';
        this.dedos_running = false;
        this.dfg_filename = false;
        this.dfg = {};
    }

    reset() {
        if (this.dedos_running) {
            ui.show_error("Shut down DeDOS before restarting");
        } else {
            socket.emit('reset');
            this.force_reset();
        }
    }

    submit_selected_dfg() {
        if (this.ui.selected_dfg != null) {
            this.ui.hide_dfg_list();
        } else {
            this.ui.show_error("Please select DFG file first");
        }
    }
}


class UI {

    constructor() {

        const dfg_params = {
            margin: 10,
            spacing: 10
        };

        this._dfg = {};
        this._runtimes = {};
        this.runtime_dimensions = [1, 1];

        this.statuses = {
            idle: '#006699',
            running: '#00cc66',
            stopping: '#ffcc00'
        }

        this.msu_colors = ["#3957ff", "#d3fe14", "#c9080a", "#fec7f8", "#0b7b3e", "#0bf0e9", "#c203c8", "#fd9b39", "#888593", "#906407", "#98ba7f", "#fe6794", "#10b0ff", "#ac7bff", "#fee7c0", "#964c63", "#1da49c", "#0ad811", "#bbd9fd", "#fe6cfe", "#297192", "#d1a09c", "#78579e", "#81ffad", "#739400", "#ca6949", "#d9bf01", "#646a58", "#d5097e"];

        this.default_dfg = 'Select a dfg';

        this.draw_menu();
        this.resize_dfg();
        $(window).on('resize', this.resize_dfg);
    }

    get menu_height() {
        return 70;
    }

    draw_menu() {
        // TOP MENU BUTTONS
        const START_COLOR = '#ff99bb';
        const END_COLOR = '#009999';

        var menu_btns = d3.select('#top_menu').selectAll('button');

        var menu_colors = chroma.scale([START_COLOR, END_COLOR])
                            .mode('lch')
                            .colors(menu_btns.size());

        const ORIG_WIDTH = 22.5; // vw

        const MENU_HEIGHT = 70 // px
        const ORIG_HEIGHT = `${MENU_HEIGHT}px`; // px
        const SELECTED_HEIGHT = `${MENU_HEIGHT+10}px` // px

        menu_btns.style('margin-left', (d, i) => { 
                      return 5 + ORIG_WIDTH * i + 'vw'; 
                  }).style('background-color', (d, i) => {
                      return menu_colors[i];
                  }).style('width', ORIG_WIDTH + 'vw')
                  .style('height', ORIG_HEIGHT)
                  .on('mouseover', (d, i) => {
                       d3.select(this).transition()
                                      .duration(500)
                                      .style('z-index', 1000)
                                      .style('height', SELECTED_HEIGHT);
                  }).on('mouseout', (d, i) => {
                       d3.select(this).transition()
                                      .duration(500)
                                      .style('z-index', 10)
                                      .style('height', ORIG_HEIGHT);
                  });
    }

    get svg_width() {
        var container = d3.select('svg').node().parentNode.parentNode;

        return 0.9 * $(container).width();
    }

    get svg_height() {
        return 0.9 * $(window).height;
    }

    get dfg_height() {
        return this.svg_height - this.menu_height;
    }

    get runtime_height() {
        return this.dfg_height / this.runtime_dimensions[1];
    }

    resize_dfg() {
        var svg = d3.select('#dfg_svg');
        svg.attr('width', this.svg_width)
           .attr('height', this.svg_height);
    }


    refresh() {
        this.app = this.app;
    }

    set app(app) {
        this.dfg = app.dfg;
    }

    set dfg(dfg) {
        this._dfg = dfg;
    }

    set msu_types(types) {
        this._msu_types = types;

        this.type_indicies = {};
        for (var i = 0; i < types.size(); i++) {
            this.type_indices[types[i].id] = i;
        }
        this.type_height = (this.runtime_height - this.dfg_params.margin * 2) / type.size() -
                           (this.dfg_params.spacing * type.size())
    }

    set runtimes(rts) {

        this._rts = rts;
        this.rt_indices = {};
        for (var i = 0; i < rts.size(); i++) {
            this.rt_indices[rts[i].id] = i;
        }

        var runtime_width = 50;
        var runtime_height = 100;

        var add_rt_label = function() {
            this.append('text')
                .attr('font-weight', 'bold')
                .attr('fill', 'white')
                .attr('x', 20)
                .attr('y', 20) //TODO
                .text((d, i) => {return `RT-${d.id}`;});
        };

        var add_rt_status = function() {
            this.append('text')
                .attr('fill', 'white')
                .attr('class', 'rt_status')
                .attr('id', (d, i) => { return `rt_status_${i}`})
                .attr('x', 20) //TODO
                .attr('y', 20) //TODO
                .attr('text-anchor', 'middle')
                .text((d, i) => {return `${d.ip} ${d.status}`});
        }

        var add_rt_status_and_label = function() {
            this.append('rect')
                .attr('class', 'rt_label_rect')
                .attr('id', (d, i) => { return `rt_label_rect_${d.id}`; })
                .attr('fill', (d, i) => { return rt_colors[d.id]; })
                .attr('height', 100) //TODO
                .attr('width', 100) //TODO
                .attr('rx', 5)
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 1);

            this.call(add_rt_status);
            this.call(add_rt_label);
        };

        var add_rt_dfg = function() {
            this.append('g')
                .attr('class', 'msu_board')
                .attr('id', (d, i) => {return `msu_board_${d.id}`;});

            this.append('rect')
                .attr('rx', 10)
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 0.2)
                .attr('height', 125) // TODO
                .attr('width', 126) // TODO
        };


        var type_height = this.type_height;
        var params = this.dfg_params;

        this._runtimes = rts;
        d3.select('#runtimes').data(rts)
            .enter()
            .append('g')
                .attr('class', 'runtime_group')
                .attr('id', (d, i) => {return `rt_group_${i}`})
                .attr('transform', (d, i) => {return i;})
                .call(add_rt_status_and_label)
                .call(add_rt_dfg)
                .data(this.msu_types)
            .enter()
            .append('g')
                .attr('class', 'type_group')
                .attr('id', (d) => {return `type_group_${d.rt.id}-${d.id}`})
                .attr('x', this.dfg_params.margin)
                .attr('y', (d, i) => {return (type_height + params.spacing) * i + params.margin;})
                .attr('width', (d) => {return thread_width(d.rt);})
                .attr('height', runtime_height - (THREAD_TOP + THREAD_BOTTOM))

        // TODO: Remake legend
    }

    get thread_width() {
        return 10;
    }

    get thread_spacing() {
        return 10;
    }

    fill_runtime_type(rt_id, type_id, msus) {
        var thread_width = this.thread_width;
        var thread_spacing = this.thread_spacing;
        var type_colors = this.type_colors;
        var msu_radius = this.msu_radius;

        var add_msu_circle = function() {
            var msu = this.append('circle');

            msu.attr('class', (d) => {return `msu msu_${d.type.id} msu_${d.id}`})
                .attr('id', (d) => {return `msu_${d.id}`})
                .attr('fill', (d) => {return type_colors[d.type.id]})
                .attr('stroke', '#aaa')
                .attr('stroke-width', 1)
                .attr('cx', 0)
                .attr('cy', 0)
                .attr('r', msu_radius)
                .attr('opacity', 0)
                .transition('msu_fade')
                .duration(500)
                .attr('opacity', 1);

            msu.exit()
                .transition("msu_fade")
                .duration(500)
                .attr('opacity', 0)
                .remove();
        }

        var add_msu_text = function() {
            this.append('text')
                .attr('class', (d) => {return `msu_label msu_${d.id}`})
                .attr('id', (d) => { return `msu_label_${d.id}`})
                .attr('x', 0)
                .attr('y', 0)
                .attr('text-anchor', 'middle')
                .attr('dominant-baseline', 'central')
                .text((d) => { return d.id })
                .attr('opacity', 0)
                .attr('font-weight', 'bold')
                .style('pointer-events', 'none')
        }

        var msus_data = d3.select(`#type_group_${rt_id}-${type_id}`).data(msus);

        msus_data.enter()
            .append('g')
                .attr('class', (d) => `msu_group msu_${d.id}`)
                .attr('id', (d) => {return `msu_group_${d.id}`;})
                .attr('x', (d) => {return d.thread_id * (thread_width + thread_spacing) + thread_spacing})
                .attr('y', 0)
                .call(add_msu_circle)
                .call(add_msu_text);

        d3.selectAll('.msu_group').on('mouseover', (d) => {
            d3.select(`#msu_label_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('opacity', 1)

            d3.select(`#msu_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('r', msu_radius * 1.1);

            d3.selectAll(`.routes_from_${d.id}`).transition('highlight').attr('opacity', 1);
        });

        d3.selectAll('.msu_group').on('mouseout', (d) => {
            d3.select(`#msu_label_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('opacity', 0)

            d3.select(`#msu_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('r', msu_radius);

            d3.selectAll(`.route_from_${d.id}`).transition('highlight').attr('opacity', .2);
        });
    }

    set msus(msus) {
        // Initialize empty structure to hold msus by runtime/type
        var by_runtime = {};
        for (var i = 0; i < this._rts.size(); i++) {
            var by_types = {};
            for (var j = 0; j < this._msu_types.size(); j++) {
                by_types[this._msu_types[j].id] = [];
            }
            by_runtime[this._rts[i].id] = by_types;
        }

        // Include the MSUs
        for (var i = 0; i < msus.size(); i++) {
            by_runtime[msus[i].runtime][msu[i].type].push(msu);
        }

        for (var i = 0; i < this._rts.size(); i++) {
            rt = this._rts[i];
            for (var j = 0; j < this._msu_types.sizre(); j++) {
                type = this._msu_types[i];
                this.fill_runtime_type(rt.id, type.id, by_runtime[rt.id][type.id]);
            }
        }
    }

    msu_location(msu) {
        m = d3.select(`#msu_${msu.id}`);
        m_gr = d3.select(m.parentNode);
        t_gr = d3.select(m_gr.parentNode);
        rt_gr = d3.select(t_gr.parentNode);

        return [m.attr('cx') + m_gr.attr('x') + t_gr.attr('x') + rt_gr.attr('x'),
                m.attr('cy') + m_gr.attr('y') + t_gr.attr('y') + rt_gr.attr('y')];
    }

    set routes(routes) {
        r = d3.select('#routes').selectAll('path').data(routes)

        r.enter()
            .append('path')
            .attr('class', (d) => { return `route .route_from_${d.src}`})
            .attr('d', (d) => {
                start = msu_location(d.src);
                end = msu_location(d.dst);
                mids = [(start[0] + end[0]) / 2, (start[1] + end[1]) / 2];
                return `M ${start[0]} ${start[1]}` +
                       `Q ${start[0]} ${mid[1]} ${mid[0]} ${mid[1]}` +
                       `Q ${end[0]} ${mid[1]} ${end[0]} ${end[1]}`;
            })
            .attr('stroke', '#dddddd')
            .attr('opacity', 0.0)
            .attr('stroke-dasharray', 500)
            .attr('stroke-dashoffset', 500)
            .transition('appearance')
            .duration(1000)
            .attr('stroke-dashoffset', 0)
            .attr('opacity', 0.2);

        r.exit()
            .transition()
            .duration(1000)
            .style('stroke-dashoffset', 500)
            .attr('opacity', 0)
            .remove()
    }

    get selected_dfg() {
        var selection = d3.select('#dfg_list').property('value');
        if (selection != this.default_dfg) {
            return selection;
        }
        return null;
    }

    set dfg_list(dfgs) {
        var entries = ([this.default_dfg]).concat(dfgs);
        d3.select('#dfg_list').selectAll('option').data(entries)
            .enter()
            .append('option')
                .text((d) => {return d});
    }

    show_dfg_list() {
        d3.select('dfg_load_window')
            .transition()
            .duration(500)
            .style('opacity', 1);
    }

    hide_dfg_list() {
        d3.select('dfg_load_window')
            .transition(500)
            .style('display', 'none');
    }

    show_error(msg) {
        d3.select('error_txt')
            .text(err);
        d3.select('error_msg').transition()
            .duration(500)
            .style('opacity', 1);
    }

    set status(stat) {
        if (stat in this.statuses) {
            d3.select('#dedos_status').select('rect')
                .transition()
                .duration(500)
                .attr('fill', this.status_colors['stat']);
        }

        d3.select('#dedos_status').select('text')
            .transition()
            .duration(500)
            .text(stat);
    }
}

ui = new UI();
var app = new Controller(ui);

var open_load_lialog = app.open_load_dialog;
var start_dedos = app.start_dedos;
var stop_dedos = app.stop_dedos;
var reset_ui = app.reset;
var submit_dfg = app.submit_selected_dfg;


$('document').ready( app.force_reset );
