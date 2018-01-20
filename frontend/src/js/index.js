var socket = io.connect();

class Controller {

    constructor(ui) {
        this.ui = ui;

        this.dfg_selected = true;
        this.dedos_running = false;
        this.app_name = false;

        socket.on('application', function(app_name) {
            this.app_name = app_name;
            ui.app_name = app_name;
        });

        socket.on('dfg_files', function(files) {
            ui.dfg_list = files;
        });

        socket.on('runtimes', function(rts) {
            ui.runtimes = rts;
        });

        socket.on('msu_types', function(msu_types) {
            ui.msu_types = msu_types;
        });

        socket.on('msus', function(msus) {
            ui.msus = msus;
        });

        socket.on('links', function(routes) {
            ui.routes = routes;
        });

        socket.on('error_msg', function(msg) {
            ui.show_error(msg);
        });

        socket.on('started', (msg) => {
            this.dedos_running = true;
            ui.status = 'running';
        });

        socket.on('stopped', (msg) => {
            this.dedos_running = false;
            this.force_reset();
        });

        d3.select('body').on('contextmenu', ui.toggle_ctx_menu.bind(ui));
        d3.select('body').on('click', ui.remove_ctx_menu.bind(ui));
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
        if (this.dedos_running) {
            this.ui.show_error("DeDOS is already running");
        } else if (!this.dfg_selected) {
            this.ui.show_error("Load DFG before starting DeDOS");
        } else {
            this.ui.status = 'starting';
            socket.emit('start', ui.dfg_filename);
        }
    }

    stop_dedos() {
        if (!this.dedos_running) {
            this.ui.show_error("DeDOS has not yet been started");
        } else {
            this.ui.status = 'stopping';
            socket.emit('stop');
        }
    }

    force_reset() {
        ui.status = 'idle';
        this.dedos_running = false;
        this.dfg_selected = false;
        ui.reset();
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
            this.dfg_selected = true;
            socket.emit('selected_dfg', this.ui.selected_dfg);
            this.ui.hide_dfg_list();
        } else {
            this.ui.show_error("Please select DFG file first");
        }
    }
}


class UI {

    constructor() {

        this._msu_types = [];
        this._rts= [];
        this._msus = [];
        this._routes = [];
        this.app_name = "";

        this.dfg_params = {
            margin: 10,
            spacing: 10,
            rt_status_height: 50
        };

        this.runtime_dimensions = [1, 1];

        this.statuses = {
            idle: '#006699',
            starting: '#33aa66',
            running: '#00cc66',
            stopping: '#ffcc00'
        }

        this.legend_visible = false;

        this.msu_radius = 15;
        this.msu_colors = ["#3957ff", "#d3fe14", "#c9080a", "#fec7f8", "#0b7b3e", "#0bf0e9", "#c203c8", "#fd9b39", "#888593", "#906407", "#98ba7f", "#fe6794", "#10b0ff", "#ac7bff", "#fee7c0", "#964c63", "#1da49c", "#0ad811", "#bbd9fd", "#fe6cfe", "#297192", "#d1a09c", "#78579e", "#81ffad", "#739400", "#ca6949", "#d9bf01", "#646a58", "#d5097e"];

        this.default_dfg = 'Select a dfg';

        this.draw_menu();
        this.resize_dfg();
        $(window).on('resize', this.resize_dfg.bind(this));
    }

    reset() {
        this.routes = [];
        this.msus = [];
        this.runtimes = [];
        this.msu_types = [];
    }

    get menu_height() {
        return 70;
    }

    toggle_legend() {
        if (this.legend_visible) {
            d3.select('#legend_toggle')
                .html('&#10097;')
                .transition()
                .duration(500)
                .style('left', '0px')
            d3.select('#legend_board')
                .transition()
                .duration(500)
                .style('left', '-120px');
            this.legend_visible = false;
        } else {
            d3.select('#legend_toggle')
                .html('&#10096;')
                .transition()
                .duration(500)
                .style('left', '120px')
            d3.select('#legend_board')
                .transition()
                .duration(500)
                .style('left', '0px')
            this.legend_visible = true;
        }
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
        const ORIG_HEIGHT = `${this.menu_height}px`; // px
        const SELECTED_HEIGHT = `${MENU_HEIGHT+10}px` // px

        menu_btns.style('margin-left', (d, i) => { 
                      return 5 + ORIG_WIDTH * i + 'vw'; 
                  }).style('background-color', (d, i) => {
                      return menu_colors[i];
                  }).style('width', ORIG_WIDTH + 'vw')
                  .style('height', ORIG_HEIGHT)
                  .on('mouseover', function (d, i) {
                       d3.select(this).transition()
                                      .duration(500)
                                      .style('z-index', 1000)
                                      .style('height', SELECTED_HEIGHT);
                  }).on('mouseout', function (d, i) {
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
        return $(window).height() - 100;
    }

    get dfg_height() {
        return this.svg_height - this.menu_height;
    }

    get dfg_width() {
        return this.svg_width;
    }

    get runtime_height() {
        return (this.dfg_height - this.status_height) / this.runtime_dimensions[0];
    }

    get status_height() {
        return 40;
    }

    get runtime_width() {
        return this.dfg_width / this.runtime_dimensions[1];
    }


    resize_dfg() {
        d3.select('#dfg_svg')
            .attr('width', this.svg_width)
            .attr('height', this.svg_height + 50)
            .attr('preserveAspectRatio', 'xMinYMin meet')
            .attr('viewBox', `0 0 ${this.svg_width} ${this.svg_height}`);

        d3.select('#dedos_status rect')
            .attr('width', this.svg_width)
            .attr('height', this.status_height);

        d3.select('#dedos_status text')
            .attr('x', this.svg_width / 2)
            .attr('y', this.status_height * .9)
            .attr('text-anchor', 'middle')
            .attr('fill', 'white');

        d3.select('#dedos_appname')
            .attr('x', this.svg_width / 2)
            .attr('y', this.status_height * .4)
            .attr('text-anchor', 'middle')
            .attr('fill', 'white');

        d3.select('#dfg')
            .attr('transform', `translate(0, ${this.status_height})`);

        d3.selectAll('.rt_bg_rect')
            .attr('height', this.runtime_height - this.dfg_params.rt_status_height);

        d3.selectAll('.msu_group')
            .attr('transform', (d) => {
                return 'translate(' +
                    (this.rt_thread_location(d.thread_id, d.runtime_id) - this.msu_radius / 2) + ',' + 
                     this.rt_type_location(d.type_id) + ')'})


        d3.selectAll('.route')
            .attr('d', (d) => {
                var start = this.msu_location(d.src);
                var end = this.msu_location(d.dst);
                var mid = [(start[0] + end[0]) / (2), (start[1] + end[1]) / 2];
                return `M ${start[0]} ${start[1]}` +
                       `Q ${start[0]} ${mid[1]} ${mid[0]} ${mid[1]}` +
                       `Q ${end[0]} ${mid[1]} ${end[0]} ${end[1]}`;
            })

        var RT_STATUS_HEIGHT = this.dfg_params.rt_status_height;

        d3.selectAll('.rt_label')
            .attr('x', this.runtime_width / 2)
            .attr('y', (d) => { return this.runtime_status_location(d.id) + RT_STATUS_HEIGHT / 3})

        d3.selectAll('.rt_status')
            .attr('x', this.runtime_width / 2)
            .attr('y', (d) => { return this.runtime_status_location(d.id) + RT_STATUS_HEIGHT * 2/3})

        d3.selectAll('.rt_label_rect')
            .attr('y', (d) => {return this.runtime_status_location(d.id)})
            .attr('height', RT_STATUS_HEIGHT) 
            .attr('width', this.runtime_width)

        d3.selectAll('.rt_bg_rect')
            .attr('height', this.runtime_height)
            .attr('width', this.runtime_width);

        d3.selectAll('.runtime_group')
            .attr('transform', (d, i) => {
                var rt_loc = this.runtime_location(d.id);
                return `translate(${rt_loc[0]},${rt_loc[1]})`});
        

        this.msu_types = this._msu_types;
    }


    refresh() {
        this.app = this.app;
    }

    set msu_types(types) {
        this._msu_types = types;

        this.type_indices = {};
        this.type_colors = {};
        for (var i = 0; i < types.length; i++) {
            this.type_indices[types[i].id] = i;
            this.type_colors[types[i].id] = this.msu_colors[i];
        }

        var msu_radius = this.msu_radius;
        var type_colors = this.type_colors;

        var size_legend_text = function(legend_g) {
            legend_g.selectAll('.legend_msu_text')
                .attr('x', msu_radius * 2 + 2)
                .attr('y', msu_radius + 5)
        }

        var add_legend_text = function(legend_g) {
            legend_g.append('text')
                .attr('class', (d) => { return 'legend_msu_text';})
                .text((d)=> { return d.name})
                .attr('dominant-baseline', 'central')
                .attr('font-weight', 'bold')
                .style('fill', 'white')
            size_legend_text(legend_g)
        }

        var size_legend_circle = function(legend_g) {
            legend_g.select('.legend_msu')
                .attr('r', msu_radius)
                .attr('cx', msu_radius)
                .attr('cy', msu_radius + 5)
        }

        var add_legend_circle = function(legend_g) {
            legend_g.append('circle')
                .attr('class', (d) => { return `legend_msu msu_type_${d.id}`})
                .attr('fill', (d) => { return type_colors[d.id]})
                .attr('stroke', '#aaa')
                .attr('stroke-width', 1);
            size_legend_circle(legend_g);
        }


        d3.select('#legend_board')
            .attr('height',(this.type_height + this.dfg_params.spacing) * Math.max(types.length, 1));

        var data = d3.select('#legend_board').selectAll('g').data(types, (d) => {return d.id});
        data
            .attr('transform',(d,i) => {
                return `translate(0, ${(this.type_height + this.dfg_params.spacing) * i})`})
            .call(size_legend_circle)
            .call(size_legend_text);
        data.enter()
            .append('g')
            .attr('class', 'legend_group msu_type')
            .attr('transform',(d,i) => {
                return `translate(0, ${(this.type_height + this.dfg_params.spacing) * i})`})
            .call(add_legend_circle)
            .call(add_legend_text);
        data.exit()
            .remove()

        d3.selectAll('.legend_group')
            .on('mouseover', (d) => {
                d3.selectAll(`.msu_label_type_${d.id}`)
                    .transition('mouseover')
                    .duration(200)
                    .attr('opacity', 1)

                d3.selectAll(`.msu_type_${d.id}`)
                    .transition('mouseover')
                    .duration(200)
                    .attr('r', msu_radius * 1.1);
                d3.selectAll(`.route_from_type_${d.id}`).transition('highlight').attr('opacity', 1);
            }).on('mouseout', (d) => {
                d3.selectAll(`.msu_label_type_${d.id}`)
                    .transition('mouseover')
                    .duration(200)
                    .attr('opacity', 0)

                d3.selectAll(`.msu_type_${d.id}`)
                    .transition('mouseover')
                    .duration(200)
                    .attr('r', msu_radius * 1);
                d3.selectAll(`.route_from_type_${d.id}`).transition('highlight').attr('opacity', .2);
            });


    }

    get type_height() {
        return (this.runtime_height - this.dfg_params.margin * 2 - this.dfg_params.rt_status_height - this.msu_radius) /
            Math.max(this._msu_types.length, 1);
    }

    runtime_location(rt_id) {
        var i = this.rt_indices[rt_id];
        var x_loc = i % this.runtime_dimensions[1];
        var y_loc = Math.floor(i / this.runtime_dimensions[1]);
        return [this.runtime_width * x_loc, this.runtime_height * y_loc];
    }

    runtime_status_location(rt_id) {
        if (Math.floor((this.rt_indices[rt_id]) / this.runtime_dimensions[1]) % 2 == 0) {
            return 0;
        } else {
            return this.runtime_height + this.dfg_params.rt_status_height;
        }
    }

    set runtimes(rts) {

        const RT_STATUS_HEIGHT = this.dfg_params.rt_status_height;

        this._rts = rts;
        this.rt_indices = {};
        for (var i = 0; i < rts.length; i++) {
            this.rt_indices[rts[i].id] = i;
        }

        if (rts.length < 5) {
            this.runtime_dimensions = [1, rts.length];
        } else {
            this.runtime_dimensions = [2, Math.floor((rts.length+1)/2)];
        }

        var runtime_width = this.runtime_width;
        var runtime_height = this.runtime_height;
	    var rt_colors = chroma.scale(['#009999','#ffcc00']).mode('lch').colors(rts.length);

        var rt_status_loc = this.runtime_status_location.bind(this);

        var set_rt_label_loc = function(rt_group) {
            rt_group
                .select('.rt_label')
                .attr('x', runtime_width / 2)
                .attr('y', (d) => { return rt_status_loc(d.id) + RT_STATUS_HEIGHT / 3})
        }

        var add_rt_label = function(rt_group) {
            rt_group.append('text')
                .attr('font-weight', 'bold')
                .attr('class', 'rt_label')
                .attr('fill', 'white')
                .attr('text-anchor', 'middle')
                .text((d, i) => {return d.ip})
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 1);

            set_rt_label_loc(rt_group)
        };

        var set_rt_status_loc = function(rt_group) {
            rt_group
                .select('.rt_status')
                .attr('x', runtime_width /2) 
                .attr('y', (d) => { return rt_status_loc(d.id) + RT_STATUS_HEIGHT * 2 / 3})
                .text((d, i) => {return d.status})
        }

        var add_rt_status = function(rt_group) {
            rt_group.append('text')
                .attr('fill', 'white')
                .attr('class', 'rt_status')
                .attr('id', (d, i) => { return `rt_status_${i}`})
                .attr('text-anchor', 'middle')
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 1);
            set_rt_status_loc(rt_group);
        }

        var set_rt_rect_loc = function(rt_group) {
            rt_group
                .select('.rt_label_rect')
                .attr('height', RT_STATUS_HEIGHT) 
                .attr('width', runtime_width)
                .attr('y', (d) => {return rt_status_loc(d.id)})
        };


        var add_rt_status_and_label = function(rt_group) {
            rt_group.append('rect')
                .attr('class', 'rt_label_rect')
                .attr('id', (d, i) => { return `rte_label_rect_${d.id}`; })
                .attr('fill', (d, i) => { return rt_colors[i]; })
                .attr('rx', 5)
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 1);

            set_rt_rect_loc(rt_group);
            rt_group.call(add_rt_status);
            rt_group.call(add_rt_label);
        };

        var set_rt_dfg_bg_size = function(rt_group) {
            rt_group
                .select('.rt_bg_rect')
                .attr('height', runtime_height)
                .attr('width', runtime_width);
        }

        var add_rt_dfg_bg = function(rt_group) {
            rt_group.append('rect')
                .attr('class', 'rt_bg_rect')
                .attr('rx', 10)
                .attr('opacity', 0)
                .transition()
                .duration(500)
                .attr('opacity', 0.2)
            
            set_rt_dfg_bg_size(rt_group)
        };

        var msu_types = this._msu_types;

        var type_height = this.type_height;
        var params = this.dfg_params;

        var d3_rts = d3.select('#dfg').selectAll('g .runtime_group').data(rts,
                function(d) { return d.id})

        d3_rts
            .transition()
            .duration(1000)
            .attr('transform', (d, i) => {
                var rt_loc = this.runtime_location(d.id);
                return `translate(${rt_loc[0]},${rt_loc[1]})`})
            .call(set_rt_rect_loc)
            .call(set_rt_status_loc)
            .call(set_rt_label_loc)
            .call(set_rt_dfg_bg_size)

        d3_rts.select('.msu_board')
            .attr('transform', `translate(0, ${RT_STATUS_HEIGHT})`)

        d3_rts.enter()
            .append('g')
                .attr('class', 'runtime_group')
                .attr('transform', (d, i) => {
                    var rt_loc = this.runtime_location(d.id);
                    return `translate(${rt_loc[0]},${rt_loc[1]})`})
                .attr('id', (d, i) => {return `rt_group_${d.id}`})
                .call(add_rt_status_and_label)
                .append('g')
                    .attr('class', 'msu_board')
                    .attr('id', (d, i) => {return `msu_board_${d.id}`;})
                    .attr('transform', `translate(0, ${RT_STATUS_HEIGHT})`)
                    .call(add_rt_dfg_bg)


        var exiting_rts = d3_rts.exit();

        exiting_rts.selectAll('rect')
            .transition()
            .duration(1000)
            .attr('opacity', 0)
            .remove();

        exiting_rts.selectAll('text')
            .transition()
            .duration(1000)
            .attr('opacity', 0)
            .remove();

        exiting_rts.selectAll('g')
            .selectAll('rect')
            .transition()
            .duration(1000)
            .attr('opacity', 0)
            .remove();

        exiting_rts.transition().delay(1000).remove();

    }

    rt_type_location(type_id) {
        var i = this.type_indices[type_id];
        return (this.type_height + this.dfg_params.spacing) * (i+.5);
    }

    rt_thread_location(thread_id, rt_id) {
        return thread_id * (this.thread_width(rt_id));
    }

    thread_width(rt_id) {
        return (this.runtime_width - this.msu_radius) / this._rts[this.rt_indices[rt_id]].threads;
    }

    get thread_spacing() {
        return 10;
    }

    fill_runtime_msus(rt_id, msus) {
        var thread_width = this.thread_width(rt_id);
        var thread_spacing = this.thread_spacing;
        var type_colors = this.type_colors;
        var msu_radius = this.msu_radius;

        var add_msu_circle = function(msu_group) {
            var msu = msu_group.append('circle');

            msu.attr('class', (d) => {return `msu msu_type_${d.type_id} msu_${d.id}`})
                .attr('id', (d) => {return `msu_${d.id}`})
                .attr('fill', (d) => {return type_colors[d.type_id]})
                .attr('stroke', '#aaa')
                .attr('stroke-width', 1)
                .attr('cx', 0)
                .attr('cy', 0)
                .attr('r', msu_radius)
                .attr('opacity', 0)
                .transition('msu_fade')
                .duration(500)
                .attr('opacity', 1);
        }

        var remove_msu_circle = function(msu_group) {
            msu_group.selectAll('circle')
                .transition("msu_fade")
                .duration(500)
                .attr('opacity', 0)
                .remove();
        }

        var add_msu_text = function(msu_group) {
            msu_group.append('text')
                .attr('class', (d) => {return `msu_label msu_${d.id} msu_label_type_${d.type_id}`})
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

        var msus = d3.select(`#msu_board_${rt_id}`).selectAll('g .msu_group').data(msus,
                (d) => { return d.id});

        msus.transition()
            .duration(1000)
            .attr('transform', (d) => {
                    return 'translate(' +
                    (this.rt_thread_location(d.thread_id, d.runtime_id) - msu_radius / 2) + ',' +
                    this.rt_type_location(d.type_id) + ')'})

        msus.enter()
            .append('g')
                .attr('class', (d) => `msu_group msu_${d.id}`)
                .attr('id', (d) => {return `msu_group_${d.id}`;})
                .attr('transform', (d) => {
                        return 'translate(' +
                        (this.rt_thread_location(d.thread_id, d.runtime_id) - msu_radius / 2) + ',' +
                        this.rt_type_location(d.type_id) + ')'})
                .call(add_msu_circle)
                .call(add_msu_text);

        msus.exit()
            .transition()
            .call(remove_msu_circle)
            .delay(1000)
            .remove()

        d3.selectAll('.msu_group').on('mouseover', (d) => {
            d3.selectAll(`#msu_label_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('opacity', 1)

            d3.select(`#msu_${d.id}`)
                .transition('mouseover')
                .duration(200)
                .attr('r', msu_radius * 1.1);

            d3.selectAll(`.route_from_${d.id}`).transition('highlight').attr('opacity', 1);
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
        this._msus = msus;
        this.msu_runtimes = {};
        this.msu_threads = {};
        this.msu_type_ids = {};

        // Initialize empty structure to hold msus by runtime/type
        var by_runtime = {};
        for (var i = 0; i < this._rts.length; i++) {
            by_runtime[this._rts[i].id] = [];
        }

        // Include the MSUs
        for (var i = 0; i < msus.length; i++) {
            by_runtime[msus[i].runtime_id].push(msus[i]);
            this.msu_runtimes[msus[i].id] = msus[i].runtime_id;
            this.msu_threads[msus[i].id] = msus[i].thread_id;
            this.msu_type_ids[msus[i].id] = msus[i].type_id;
        }

        for (var i = 0; i < this._rts.length; i++) {
            var rt = this._rts[i];
            this.fill_runtime_msus(rt.id, by_runtime[rt.id]);
        }
    }

    msu_location(msu) {
        var rt_loc = this.runtime_location(this.msu_runtimes[msu]);
        return [this.rt_thread_location(this.msu_threads[msu], this.msu_runtimes[msu]) + rt_loc[0],
                this.rt_type_location(this.msu_type_ids[msu]) + rt_loc[1] + this.dfg_params.rt_status_height];
    }

    set routes(routes) {
        this._routes = routes;
        var r = d3.select('#routes').selectAll('path').data(routes,
                (d) => {return `${d.src}-${d.dst}`});

        r.attr('d', (d) => {
                var start = this.msu_location(d.src);
                var end = this.msu_location(d.dst);
                var mid = [(start[0] + end[0]) / (2), (start[1] + end[1]) / 2];
                return `M ${start[0]} ${start[1]}` +
                       `Q ${start[0]} ${mid[1]} ${mid[0]} ${mid[1]}` +
                       `Q ${end[0]} ${mid[1]} ${end[0]} ${end[1]}`;})
            .attr('stroke-dasharray', function() { return this.getTotalLength()} )
            .attr('stroke-dashoffset', 0)

        r.enter()
            .append('path')
            .attr('class', (d) => { return `route route_from_${d.src} route_from_type_${this.msu_type_ids[d.src]}`})
            .attr('d', (d) => {
                var start = this.msu_location(d.src);
                var end = this.msu_location(d.dst);
                var mid = [(start[0] + end[0]) / (2), (start[1] + end[1]) / 2];
                return `M ${start[0]} ${start[1]}` +
                       `Q ${start[0]} ${mid[1]} ${mid[0]} ${mid[1]}` +
                       `Q ${end[0]} ${mid[1]} ${end[0]} ${end[1]}`;
            })
            .attr('stroke', '#dddddd')
            .attr('opacity', 0.0)
            .attr('stroke-dasharray', function() { return this.getTotalLength()} )
            .attr('stroke-dashoffset', function() { return this.getTotalLength()})
            .transition('appearance')
            .duration(1000)
            .attr('stroke-dashoffset', 0)
            .attr('opacity', 0.2);

        r.exit()
            .transition()
            .duration(1000)
            .style('stroke-dashoffset', function() { return this.getTotalLength();})
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
        d3.select('#dfg_loader')
            .style('display', 'inline')
            .transition()
            .duration(500)
            .style('opacity', 1);

    }

    hide_dfg_list() {
        d3.select('#dfg_loader')
            .transition()
            .duration(500)
            .style('opacity', 0)
            .style('display', 'none');
    }

    show_error(msg) {
        d3.select('#error_txt')
            .text(msg);
        d3.select('#error_msg')
            .style('display', 'inline')
            .transition()
            .duration(500)
            .style('opacity', 1);
    }

    hide_error(msg) {
        d3.select('#error_msg')
            .transition()
            .duration(500)
            .style('opacity', 0)
            .style('display', 'none');
    }

    set app_name(name) {
        d3.select('#dedos_appname')
            .text(name)
    }

    set status(stat) {
        if (stat in this.statuses) {
            d3.select('#dedos_status').select('rect')
                .transition()
                .duration(500)
                .attr('fill', this.statuses[stat]);
        }

        d3.select('#dedos_status').select('text')
            .transition()
            .duration(500)
            .text(stat);
    }

    toggle_ctx_menu() {
        if (this.ctx_menu_visible) {
            d3.event.preventDefault();
            this.remove_ctx_menu();
        } else {
            var d3_target = d3.select(d3.event.target);
            // If it's not an MSU, do nothing
            if (!d3_target.classed('msu')) {
                return;
            } else {
                d3.event.preventDefault();
                var msu = d3_target.datum();

                var canvas = d3.select('#dfg');
                var msu_loc = this.msu_location(msu.id);

                this.draw_ctx_menu(canvas,
                              msu_loc[0],
                              msu_loc[1],
                              msu.id);
            }
        }
    }

    remove_ctx_menu() {
        if (this.ctx_menu_visible) {
            d3.select('#ctx_menu').remove();
            this.ctx_menu_visible = false;
        }
    }


    draw_ctx_menu(canvas, x, y, id) {
        this.ctx_menu_visible = true;

        const MENU_WIDTH = 70;
        const MENU_HEIGHT = 60;
        const MENU_ROUND = 10;

        const COLOR_OUT = 'rgba(255,255,255,.6)';
        const COLOR_IN = 'rgba(255,255,255,1)';

        var ctx_menu = canvas.append('g')
                    .attr('id', 'ctx_menu')

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
            this.remove_ctx_menu();
        });

        // Creating the REMOVE button
        var unclone_group = ctx_menu.append('g');

        var unclone_rect = unclone_group.append('rect')
                       .attr('class', 'ctx_menu_button')
                       .attr('width', MENU_WIDTH)
                       .attr('height', MENU_HEIGHT/2)
                       .attr('x', x)
                       .attr('y', y + MENU_HEIGHT/2)
                       .attr('rx', MENU_ROUND)
                       .attr('ry', MENU_ROUND);

        unclone_group.append('text')
                         .attr('x', x + MENU_WIDTH / 2)
                         .attr('y', y + MENU_HEIGHT * .75)
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

}

ui = new UI();
var app = new Controller(ui);

var open_load_dialog = app.open_load_dialog.bind(app);
var start_dedos = app.start_dedos.bind(app);
var stop_dedos = app.stop_dedos.bind(app);
var reset_ui = app.reset.bind(app);
var submit_dfg = app.submit_selected_dfg.bind(app);
var close_loader = ui.hide_dfg_list.bind(ui);
var hide_error = ui.hide_error.bind(ui);
var toggle_legend = ui.toggle_legend.bind(ui);

$('document').ready( app.force_reset );
