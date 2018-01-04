var socket = io.connect();

//drawing parameters
var drawing_params = {
	dfg_height:0,
	dfg_width:0,
	ms_height:0,
	board_width:0,
	board_height:0,
	pool_margin:0,
	nodes_spacing:0,
	layer_margin:0,
	gap:5,
	ratio:1
};

var status_color = {
	"idle":'#006699',
	"running":'#00cc66',
	"stopping":'#ffcc00',
	"attacking":'#ff3333',
};

var attack_strength = ["low","medium","high"];
var tag_color = ['#006699','#ff9966'];
var ex_color = d3.scaleQuantile().domain([0,1])
        .range(["#33cc33","#ffb31a","#ff471a"]); 

var disc_timer;		//Timer for disconnect status that used to display a gradient effect
var state_flag = 0;	//"0" for unload;"1" for file loaded but not start the experiment;"2" for file loaded and experiment started but not everything launched;"3" for everything including all runtimes and experiment launched
var filename = "";	//filename of the loaded dfg json file
var launched_num = 0; 	//number of launched runtime
var rt_num = 0; 	//number of runtime
var cores = [];
var type_code = [];
var type_name = [];

var rt_info = [];
var radius = []

var menu_btn_num = 0;

var menu_colors;
var msu_color = ["#3957ff", "#d3fe14", "#c9080a", "#fec7f8", "#0b7b3e", "#0bf0e9", "#c203c8", "#fd9b39", "#888593", "#906407", "#98ba7f", "#fe6794", "#10b0ff", "#ac7bff", "#fee7c0", "#964c63", "#1da49c", "#0ad811", "#bbd9fd", "#fe6cfe", "#297192", "#d1a09c", "#78579e", "#81ffad", "#739400", "#ca6949", "#d9bf01", "#646a58", "#d5097e"];


var rt_colors;
var rt_status = ["Idle","Launching","Launched"];

var context_menu_showing = false;

d3.select('body').on('click', function() {
    if (context_menu_showing) {
        d3.select('.popup').remove();
        context_menu_showing = false;
    }
});

d3.select('body').on('contextmenu', function(d, i) {
    if (context_menu_showing) {
        d3.event.preventDefault();
        d3.select('.popup').remove();
        context_menu_showing = false;
    } else {
        d3_target = d3.select(d3.event.target);
        if (d3_target.classed('node')) {
            d3.event.preventDefault();
            context_menu_showing = true;

            node = d3_target.datum();

            canvas = d3.select(d3_target.node().parentNode);

            popup = canvas.append('g')
                .attr('class', 'popup')

            popup.append("rect")
                .attr('class', 'popup_bg')
                .attr('width', 70)
                .attr('height', 60)
                .attr('x', d3_target.attr('cx'))
                .attr('y', d3_target.attr('cy'))
                .attr('rx', '10')
                .attr('ry', '10')

            clone_group = popup.append('g')

            clone_rect = clone_group.append('rect')
                .attr('class', 'popup_button')
                .attr('width', 70)
                .attr('height', 30)
                .attr('x', d3_target.attr('cx'))
                .attr('y', d3_target.attr('cy'))
                .attr('rx', '10')
                .attr('ry', '10')

            clone_group.append('text')
                .attr('x', parseInt(d3_target.attr('cx')) + 35)
                .attr('y', parseInt(d3_target.attr('cy')) + 15)
                .attr('text-anchor', 'middle')
                .attr('dominant-baseline', 'central')
                .text(`Clone`)

            clone_group.on('mouseover', function() {
                clone_rect.transition()
                    .duration(200)
                    .style('fill', 'rgba(255, 255, 255, 1)')
            }).on('mouseout', function() {
                clone_rect.transition()
                    .duration(200)
                    .style('fill', 'rgba(255, 255, 255, .6)')
            }).on('click', function() {
                socket.emit('clone', node.id);
                d3.select('.popup').remove();
                context_menu_showing = false;
            });

            unclone_group = popup.append('g')

            unclone_rect = unclone_group.append('rect')
                .attr('class', 'popup_button')
                .attr('width', 70)
                .attr('height', 30)
                .attr('x', d3_target.attr('cx'))
                .attr('y', parseInt(d3_target.attr('cy')) + 30)
                .attr('rx', '10')
                .attr('ry', '10')

            unclone_group.append('text')
                .attr('x', parseInt(d3_target.attr('cx')) + 35)
                .attr('y', parseInt(d3_target.attr('cy')) + 45)
                .attr('text-anchor', 'middle')
                .attr('dominant-baseline', 'central')
                .text(`Remove`)

            unclone_group.on('mouseover', function() {
                unclone_rect.transition()
                    .duration(200)
                    .style('fill', 'rgba(255, 255, 255, 1)')
            }).on('mouseout', function() {
                unclone_rect.transition()
                    .duration(200)
                    .style('fill', 'rgba(255, 255, 255, .6)')
            }).on('click', function() {
                socket.emit('unclone', node.id);
                d3.select('.popup').remove();
                context_menu_showing = false;
            });

            // This brings the parent node to the front
            canvas.node().parentNode.appendChild(canvas.node())
        }
    }
});

//initialize the layout for the page 
$('document').ready(function(){
	menu_btn_num = $('.level button').length
	menu_colors = chroma.scale(['#ff99bb','#009999']).mode('lch').colors(menu_btn_num);
	$('.level').each(function(i,e){
		jQuery(this).find('button').each(function(j,ee){
			$(this).css('margin-left',5 + 22.5 * j+'vw');	
			$(this).css('left',-5*j);
			$(this).css('background-color',menu_colors[j]);
		});
	});
	$('.level button').mouseover(function(){
		$('.level button').css('transition','0.5s all');
		$(this).css('z-index',1000);
		$(this).css('height','80px');
		$(this).css('transition-delay','0s');
	});
	$('.level button').mouseout(function(){
		$(this).css('z-index',10);
		$(this).css('height','70px');
	});

	var num = $('.btn-content').length/2;
	$('.btn-content').each(function(i,d){
		$(d).css('background-color',tag_color[Math.floor((i/2))]);
		var temp = num * 60 / 2 - Math.floor((i / 2)) * 60;
		if(temp==0)
			$(d).css('top','50%');
		else if(temp<0)
			$(d).css('top','calc(50% + '+(-1*temp)+'px)');
		else
			$(d).css('top','calc(50% - '+temp+'px)');
	});
	$('.legend-board').each(function(i,d){
		$(d).css('background-color',tag_color[i]);
	});

	$('.toggle-on').click(function(){
		$('.btn-content').css('left','10vw');
		$('.legend-wrapper').css('left','0');
	});
	$('.toggle-off').click(function(){
		$('.btn-content').css('left','0');
		$('.legend-wrapper').css('left','-10vw');
	});

	$('#toggle-on-l').click(function(){
		$('#legend-wrapper-l').css('z-index',10);
		l_on();
	});
	$('#toggle-off-l').click(function(){
		l_off();				
	});

	//legend menu toggle off
	function l_off(){
		$('#toggle-off-l').css('z-index','0');
		$('#toggle-off-l').css('opacity','0');
		$('#toggle-on-l').css('z-index','10');
		$('#toggle-on-l').css('opacity','1');
	}

	//legend menu toggle on
	function l_on(){
		$('#toggle-on-l').css('z-index','0');
		$('#toggle-on-l').css('opacity','0');
		$('#toggle-off-l').css('z-index','10');
		$('#toggle-off-l').css('opacity','1');
	}
});

var init_width = jQuery(window).width();
var init_height = Math.max(800,jQuery(window).height());

drawSVG();

//Onclick function
/**
 * transmit the command to the server to fetch the all available dfg file
 * @params: none
 * @return: none		 
 */
function loadFile(){
	if(state_flag < 2){
		socket.emit('load-file','placeholder');
		$('select option').each(function(i,e){
			if(i>0)
				$(this).remove();
		});
		$('#container').fadeIn(500);
		$('#load_file').css('display','block');
	}
	else{
		var msg = 'Please stop the experiment first!';
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
}

/**
 * start the dedos program(global controller, all runtimes and benchmarking tool as client)
 * @params: none
 * @returm: none
 */
function start(){
	if(state_flag == 0){
		var msg = 'Please load a dfg file first!';
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else if(state_flag >= 3){
		var msg = 'Please stop the experiment first!';
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else{
		socket.emit('start',filename);
	}
}
function stop(){
	if(state_flag<2){
		var msg = "No runtime launched yet!";
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else if(state_flag == 2){
		var msg = "Please wait for all runtimes launched!";
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else{
		socket.emit('stop','placeholder');
		d3.select('#status_board').select('rect')
			.transition()
			.duration(500)
			.attr('fill',status_color['stopping']);
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Stopping');
	}
}

function reset(){
	if(state_flag == 1){
		d3.select('#status_board').select('rect')
			.transition()
			.duration(500)
			.attr('fill',status_color['idle']);
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Idle');
		d3.select('svg').remove();
		drawSVG();
		state_flag = 0;
		$('.legend-wrapper').css('display','none');
		$('.btn-content').css('display','none');
		$('.content').remove();

		socket.emit('reset','placeholder');
	}
	else if(state_flag > 1){
		var msg = "Please stop the experiment first";
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
}

function submit(){
	if($(".wrapper select option").length<2){
		var msg = "There is no valid dfg file";
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else if($(".wrapper select option:selected").val()==""){
		var msg = "Please select an dfg file!";
		$('#container').fadeIn(500);
		$('#error_msg').css('display','block');
		$('#warning_txt').text(msg);
	}
	else{
		launched_num = 0;	//reset the number of launched runtime to zero
		filename = $(".wrapper select option:selected").val();
		socket.emit('dfg_file',filename);
		$('.exp_window').css('display','none');
		$('#container').css('display','none');
	}
}

function cancel(){
	$('#container').css('display','none');
	$('.exp_window').css('display','none');
}

//Drawing function
function drawSVG(){
	var svg = d3.select('body')
			.append('div').classed('canvas', true)
			.append('svg')
			.attr("preserveAspectRatio", "xMinYMin meet")
			.attr("viewBox", "0 0 " + (0.9 * init_width - (menu_btn_num - 1) * 5) + " " + (init_height - 120))
			.classed("svg-content-responsive", true) 
			.attr('width',0.9 * init_width - (menu_btn_num - 1) * 5)
			.attr('height',init_height - 120)
			.append('g');
	//drawing size initialize
	drawing_params.board_height = 0.08*init_height;		//define board height

	var chart = $("svg"),
	    aspect = chart.width() / chart.height(),
	    container = chart.parent().parent();
	$(window).on("resize", function() {
	    var targetWidth = 0.9 * container.width() - (menu_btn_num - 1) * 5;
	    chart.attr('transform','translate(' + 0.05 * container.width() + ',' + '90)');
	    chart.attr("width", targetWidth);
	    chart.attr("height", Math.round(targetWidth / aspect));
	}).trigger("resize");
	
	//draw status board
    var board_g = svg.append('g')
        .attr('id','status_board')
        .attr('transform','translate(0,0)')
        .attr('opacity',1);
    board_g.append('rect')
        .attr('width',0.9 * init_width - (menu_btn_num - 1) * 5)
        .attr('height',drawing_params.board_height)
        .attr('fill',status_color['idle'])
        .attr('rx',5);
   	board_g.append('text')
	   	.attr('x',(0.9 * init_width - (menu_btn_num - 1) * 5) / 2)
	   	.attr('y',drawing_params.board_height / 2)
	   	.attr('text-anchor','middle')
	   	.attr('baseline-shift','-24%')
	   	.attr('fill','white')
	   	.text('Idle');
}

function drawInit(rt_info,i){
	rt_colors = chroma.scale(['#009999','#ffcc00']).mode('lch').colors(rt_num);

	if(rt_num<5){
		drawing_params.dfg_height = init_height - 120 - 2 * (drawing_params.board_height + drawing_params.gap);
	}
	else{
		drawing_params.dfg_height = (init_height - 120 - (drawing_params.board_height + drawing_params.gap) - drawing_params.gap)/2 - (drawing_params.board_height + drawing_params.gap);	
	}
	drawing_params.pool_margin = 0.005*Math.max(init_height,init_width);
	drawing_params.layer_margin = 1.5*drawing_params.pool_margin;
	drawing_params.nodes_spacing = 0.025*drawing_params.pool_margin;
	if(rt_num<5){
		drawing_params.board_width = (0.9*init_width - (menu_btn_num - 1) * 5 - (rt_num - 1)*drawing_params.gap)/rt_num;
		drawing_params.dfg_width = drawing_params.board_width;
	}
	else{
		drawing_params.board_width = (0.9*init_width - (menu_btn_num - 1) * 5 - (Math.ceil(rt_num/2) - 1)*drawing_params.gap)/Math.ceil(rt_num/2);
		drawing_params.dfg_width = drawing_params.board_width;
	}

	var rt_board = d3.select('svg')
		.append('g')
		.attr('transform',function(){
			if(rt_num<5)
				return 'translate('+i*(drawing_params.board_width+drawing_params.gap)+','+(drawing_params.board_height+drawing_params.gap)+')';
			else{
				if(i<Math.ceil(rt_num/2)){
					return 'translate('+i*(drawing_params.board_width+drawing_params.gap)+','+(drawing_params.board_height+drawing_params.gap)+')';
				}
				else{
					if(rt_num%2==1){
						var sec_layer_w = drawing_params.board_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
						var offset_y = 2*(drawing_params.board_height+drawing_params.gap)+2*(drawing_params.dfg_height+drawing_params.gap);
						return 'translate('+(i%(Math.ceil(rt_num/2)))*(sec_layer_w+drawing_params.gap)+','+offset_y+')';
					}
					else{
						var offset_y = 2*(drawing_params.board_height+drawing_params.gap)+2*(drawing_params.dfg_height+drawing_params.gap);
						return 'translate('+(i%(rt_num/2))*(drawing_params.board_width+drawing_params.gap)+','+offset_y+')';
					}
				}
			}
		})
		.attr('class','rt_board')
		.attr('id','rt_board_'+i);
	rt_board.append('rect')
		.attr('id','rt_rect_'+i)
		.attr('opacity',0)
		.attr('height',0)
		.attr('width',0)
		.attr('fill',rt_colors[i])
		.attr('rx',5)
		.attr('height',drawing_params.board_height)
		.attr('width',function(){
			if(rt_num<5||rt_num>=5&&i<Math.ceil(rt_num/2))
				return drawing_params.board_width;
			else{
				if(rt_num%2==1)
					return drawing_params.board_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
				else
					return drawing_params.board_width;
			}
		})
		.transition()
		.duration(500)
		.attr('opacity',1);
	rt_board.append('text')
		.attr('font-weight','bold')
		.attr('fill','white')
		.attr('x',20)
		.attr('y',drawing_params.board_height/2)
		.attr('baseline-shift','-24%')
		.html('RT-'+rt_info[i].id);
	rt_board.append('text')
		.attr('fill','white')
		.attr('class','ip_n_status')
		.attr('id','ip_n_status_'+i)
		.attr('x',function(){
			if(rt_num<5||rt_num>=5&&i<Math.ceil(rt_num/2))
				return 70+(drawing_params.board_width - 20 - 70)/2;
			else{
				if(rt_num%2==1){
					var sec_layer_w = drawing_params.board_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
					return 70+(sec_layer_w - 20 - 70)/2;
				}
				else
					return 70+(drawing_params.board_width - 20 - 70)/2;
			}
		})
		.attr('y',drawing_params.board_height/2)
		.attr('text-anchor','middle')
		.attr('baseline-shift','-24%')
		.html(rt_info[i].ip+'&ensp;'+rt_info[i].status);
	//dfg_pool
	var rt_board = d3.select('svg')
		.append('g')
		.attr('transform',function(){
			if(rt_num<5)
				return 'translate('+i*(drawing_params.board_width+drawing_params.gap)+','+2*(drawing_params.gap+drawing_params.board_height)+')';
			else{
				if(i<Math.ceil(rt_num/2))
					return 'translate('+i*(drawing_params.board_width+drawing_params.gap)+','+2*(drawing_params.gap+drawing_params.board_height)+')';
				else{
					if(rt_num%2==1){
						var sec_layer_w = drawing_params.board_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
						var offset_y = 2*(drawing_params.board_height+drawing_params.gap)+drawing_params.dfg_height+drawing_params.gap;
						return 'translate('+(i%(Math.ceil(rt_num/2)))*(sec_layer_w+drawing_params.gap)+','+offset_y+')';
					}
					else{
						var offset_y = 2*(drawing_params.board_height+drawing_params.gap)+drawing_params.dfg_height+drawing_params.gap;
						return 'translate('+(i%(rt_num/2))*(drawing_params.board_width+drawing_params.gap)+','+offset_y+')';
					}
				}
			}
		})
		.attr('class','dfg')
		.attr('id','pool_'+i);
	rt_board.append('rect')
		.attr('opacity',0)
		.attr('rx',10)
		.attr('height',0)
		.attr('width',0)
		.attr('fill','#888')
		.transition()
		.duration(500)
		.attr('opacity',0.2)
		.attr('height',drawing_params.dfg_height)
		.attr('width',function(){
			if(rt_num<5||rt_num>=5&&i<Math.ceil(rt_num/2))
				return drawing_params.board_width;
			else{
				if(rt_num%2==1)
					return drawing_params.board_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
				else
					return drawing_params.board_width;
			}
		});
}

function node_x(d, i, runtime_id, num) {
    var pos_index = d['thread_id'] - 1;
    if(rt_num<5||rt_num>=5&&rt_num%2==0){
        var val = ((drawing_params.dfg_width - drawing_params.pool_margin*2)/num)*(pos_index+0.5)+drawing_params.pool_margin;
        return val;
    }else{
        if(runtime_id<Math.ceil(rt_num/2))
            return ((drawing_params.dfg_width - drawing_params.pool_margin*2)/num)*(pos_index+0.5)+drawing_params.pool_margin;
        else{
            var sec_layer_w = drawing_params.dfg_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
            return ((sec_layer_w - drawing_params.pool_margin*2)/num)*(pos_index+0.5)+drawing_params.pool_margin;
        }
    }
}

function node_y(type_num, type, types) {
    return (drawing_params.dfg_height - drawing_params.pool_margin)/type_num*(typeMapping(type,types)+0.5)+drawing_params.pool_margin; 
}

function node_mouseover(d, radius) {
    d3.select(`#node_label_${d.id}`)
        .transition("mouseover")
        .duration(200)
        .attr("opacity", 1);

    d3.select(`#node_${d.id}`)
        .transition("mouseover")
        .duration(200)
        .attr("r", radius*1.1);
    
    d3.selectAll('.outgoing-' + d.id).transition("highlight").attr('opacity',1);
}

function node_mouseout(d, radius) {
    d3.select(`#node_label_${d.id}`)
        .transition("mouseover")
        .duration(200)
        .attr("opacity", 0);

    d3.select(`#node_${d.id}`)
        .transition("mouseover")
        .duration(200)
        .attr("r", radius);
    
    d3.selectAll('.outgoing-' + d.id).transition("highlight").attr('opacity',0.2);
}
    

function drawNodesByType(selection,classified_data,radius,type,num,runtime_id,index,types,mode){
    var type_num = types.length;
    if(document.getElementById('box_'+type+"_"+runtime_id)==null){
        var box = selection.append('rect')
            .attr("x",drawing_params.pool_margin)
            .attr("y",function(){
                return (drawing_params.dfg_height - drawing_params.pool_margin)/type_num*typeMapping(type,types)+drawing_params.pool_margin+0.5*drawing_params.layer_margin;
            })
            .attr("width",function(){
            	if(rt_num<5||rt_num>=5&&rt_num%2==0)
                    return drawing_params.dfg_width - (drawing_params.pool_margin*2);
                else{
                	if(runtime_id<Math.ceil(rt_num/2))
                        return drawing_params.dfg_width - (drawing_params.pool_margin*2);
                	else{
						var sec_layer_w = drawing_params.dfg_width*Math.ceil(rt_num/2)/parseInt(rt_num/2);
						return sec_layer_w - (drawing_params.pool_margin*2);
                	}
                }
            })
            .attr("height",function(){
                return (drawing_params.dfg_height - drawing_params.pool_margin)/type_num - drawing_params.layer_margin;
            })
            .attr('id','box_'+type+"_"+runtime_id)
            .attr("stroke","#aaa")
            .attr("stroke-width",2)
            .attr("stroke-dasharray", "4,4")
            .attr("fill-opacity",0)
            .attr('opacity',0.4);
    }

    var node = selection.selectAll(".node_"+type)
            .data(classified_data[runtime_id][type]);

    node.enter().append("circle")
            .attr('class','node node_'+type+' node_rt_'+runtime_id)
            .attr("id", function(d){
                return "node_" + d.id;
            })
            .attr('fill',function(){
        	    return msu_color[typeMapping(type,types)];
            })
            .attr('stroke',function(){
                return '#ddd';
            })
            .attr('stroke-width',radius*0.25)
            .attr('cx', function (d,i) {
                return node_x(d, i, runtime_id, num);
            })
            .attr('cy', function () { 
                return node_y(type_num, type, types);
            })
            .attr('r',radius)
            .attr('opacity',0)
            .transition("node_appearance")
            .duration(500)
            .attr('opacity',1)

    node.enter().append('text')
        .attr('class', 'node_label')
        .attr('id', function(d) {
            return 'node_label_' + d.id;
        })
        .attr('x', function(d, i) {
            return node_x(d, i, runtime_id, num);
        })
        .attr('y', function() {
            return node_y(type_num, type, types);
        })
        .attr('text-anchor', 'middle')
        .attr('dominant-baseline', 'central')
        .text(function(d) {
            return d.id;
        })
        .attr('opacity', 0)
        .attr('font-weight', 'bold')
        .style('pointer-events', 'none');


    node.exit()
    .transition("node_appearance")
    .duration(500)
    .attr('opacity',0)
    .remove();



    selection.selectAll(".node_"+type).on('mouseover', (d)=>{return node_mouseover(d, radius)});
    selection.selectAll(".node_label").on('mouseover', (d)=>{return node_mouseover(d, radius)});
            /*
            function(d){
        node_mouseover(d.id);
        node = d3.select(this)
            .transition("node_mouseover")
            .duration("200")
            .attr("r",radius*1.1);


        //group = d3.select(this.parentNode);
        //group.append('text')
        //    .attr('x', node.attr('cx'))
        //    .attr('y', node.attr('cy'))
        //    .text(d.id)

        d3.selectAll('.outgoing-' + d.id).transition("highlight").attr('opacity',1);
    });*/

    selection.selectAll(".node_"+type).on('mouseout', (d) => {return node_mouseout(d, radius)});
    selection.selectAll(".node_label").on('mouseout', (d) => {return node_mouseout(d, radius)});
}

function drawLinks(links){
	var svg = d3.select('svg').select('g');
    var curveData = [];
    //Links in dfg preprocess
    links.forEach(function(e){
        var src_node = d3.select('#node_'+e.source);
        var parent_node = src_node.select(function(){return this.parentNode})
        if (parent_node.empty()) {
            return;
        }
        var src_translation = src_node.select(function(){return this.parentNode}).attr('transform');
        src_translation = translateParse(src_translation);
        var temp_s = {
            "x":parseInt(d3.select('#node_'+e.source).attr("cx"))+parseInt(src_translation.x),
            "y":parseInt(d3.select('#node_'+e.source).attr("cy"))+parseInt(src_translation.y),
            "r":d3.select('#node_'+e.source).attr("r"),
            "id":e.source
        };
        d3_node = d3.select("#node_" + e.target)
        if (d3_node.empty()) {
            return
        }

        className = d3.select("#node_"+e.target).attr('class');
        className = className.substring((className.length-3),className.length);
        var dst_node = d3.select('#node_'+e.target);
        var dst_translation = dst_node.select(function(){return this.parentNode}).attr('transform');
        dst_translation = translateParse(dst_translation);
        var temp_t = {
            "x":parseInt(d3.select('#node_'+e.target).attr("cx"))+parseInt(dst_translation.x),
            "y":parseInt(d3.select('#node_'+e.target).attr("cy"))+parseInt(dst_translation.y),
            "r":d3.select('#node_'+e.target).attr("r"),
            "id":e.target
        };
        var temp = [temp_s,temp_t];
        curveData.push(temp);
    }); 
    //Draw links in dfg
    var link = d3.select('svg').select('g').selectAll(".link")
            .data(curveData, (d) => { return d[0].id + "," + d[1].id });
    
    link.enter()
        .append("path")
        .attr("class", function(d){
        	return "link outgoing-"+d[0].id;
        })
        .attr("d", function(d){
            if(d[0].y>=d[1].y)
                return;
            if(d[0].x==d[1].x){
                return "M" + d[0].x + "," + (parseInt(d[0].y)+parseInt(d[0].r)) 
                + "L" + d[0].x+ ","
                + (parseInt(d[1].y) + d[0].y)/2 
                + "L" + d[0].x + ","
                + (parseInt(d[1].y) - parseInt(d[1].r));
            }
            else{
                return "M" + d[0].x + "," + (parseInt(d[0].y) + parseInt(d[0].r))
                + "T" + (parseInt(d[1].x)+d[0].x)/2 + "," + (0.4*parseInt(d[1].y)+0.6*d[0].y)
                + "T" + d[1].x + "," + (parseInt(d[1].y) - parseInt(d[1].r));   
            }
        })
        .attr('stroke','#ddd')
        .attr('opacity',0.0)
        .transition("appearance")
        .duration(500)
        .attr('opacity', 0.2);

    link.exit()
        .transition("appearance")
	    .duration(500)
	    .attr('opacity',0)
	    .remove();

    link.transition("appearance")
    	.attr('class',function(d){
        	return "link outgoing-"+d[0].id;
    	})
        .duration(500)
        .attr("d", function(d){
            if(d[0].y>=d[1].y)
                return;
            if(d[0].x==d[1].x){
                return "M" + d[0].x + "," + (parseInt(d[0].y)+parseInt(d[0].r)) 
                + "L" + d[0].x+ ","
                + (parseInt(d[1].y) + d[0].y)/2 
                + "L" + d[0].x + ","
                + (parseInt(d[1].y) - parseInt(d[1].r));
            }
            else{
                return "M" + d[0].x + "," + (parseInt(d[0].y) + parseInt(d[0].r))
                + "T" + (parseInt(d[1].x)+d[0].x)/2 + "," + (0.4*parseInt(d[1].y)+0.6*d[0].y)
                + "T" + d[1].x + "," + (parseInt(d[1].y) - parseInt(d[1].r));   
            }
    });


}

//Assistance parsing function
function translateParse(str){
    var res = {
        x:0,
        y:0
    };
    var temp = str;
    var translate_str = "translate(";
    res.x = str.substring(translate_str.length,str.indexOf(','));
    res.y = str.substring(str.indexOf(',')+1,str.length-1);
    return res;
}

function typeMapping(type,types){
	for(var i = 0;i<types.length;i++){
		if(types[i].type==type)
			return types[i].order;
	}
	return -1;
}

function getIpAddress(str){
	var pos = -1;
	rt_status.forEach(function(e){
    	pos = Math.max(pos,str.indexOf(e));
	});
	return str.substring(0,pos-1);
}

socket.on('connect',function(){
	console.log('Connect successfully');
	if(document.getElementById('status_board')!=null){
		clearInterval(disc_timer);
		d3.select('#status_board').select('rect')
			.transition()
			.duration(500)
			.attr('fill',status_color['idle']);
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Idle');
		d3.select('svg').remove();
		drawSVG();
		$('.legend-wrapper').css('display','none');
		$('.btn-content').css('display','none');
		$('.content').remove();
		state_flag = 0;
	}
});

socket.on('dfg_files',function(data){
	for(var i = 0;i<data.length;i++){
		var str = "<option value='"+data[i]+"'>"+data[i]+"</option>";
		$('select').append(str);
	}
});

socket.on('error_msg',function(data){
	$('#container').fadeIn(500);
	$('#error_msg').css('display','block');
	$('#warning_txt').text(data);
});

socket.on('rt_info',function(data){
	rt_info = data;
	d3.selectAll('.rt_board').remove();
	d3.selectAll('.dfg').remove();
	d3.selectAll('.content').remove();
	d3.selectAll('.link').remove();
	d3.selectAll('.ms_board').remove();
	d3.selectAll('.machine_stat').remove();
	rt_num = rt_info.length;
	
	for(var i = 0;i<rt_num;i++){
		drawInit(rt_info,i);
	}
	var temp = [];
	d3.selectAll('.rt_board').each(function(e){
		temp.push(translateParse(d3.select(this).attr('transform')));
	});
	d3.selectAll('.rt_board').on('mouseover',function(d,i){
		d3.select(this).select('rect')
			.attr('opacity',1)
			.transition()
			.duration(100)
			.attr('fill','#336699');
	});
	d3.selectAll('.rt_board').on('mouseout',function(d,i){
		d3.select(this).select('rect')
			.transition()
			.duration(100)
			.attr('fill',rt_colors[i]);
	});
});

socket.on('dfg',function(data){
	var nodes = data.nodes;
	var links = data.links;
	var classified_data = data.classified_data;


	var types = data.types;
	var mode = data.mode;
	
	filename = data.filename;
	cores = data.cores;
	radius = [];
	type_code = [];
	type_name = [];
	types.forEach(function(e){
		type_code.push(e['type']);
		type_name.push(e['name']);
	});

	$('.legend-wrapper').css('display','block');
	$('.btn-content').css('display','block');

	if(mode == 'init'){
		state_flag = 1;
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Idle   ' + filename);
	}
	else{
		state_flag = 3;
		if(mode != 'normal')
			launched_num = rt_info.length;
		d3.select('#status_board').select('rect')
			.transition()
			.duration(500)
			.attr('fill',status_color['running']);
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Running    ' + filename);
		var index = data;
		for(var i = 0;i<rt_info.length;i++){
			d3.select('#ip_n_status_'+i).html(function(){
				var ip = rt_info[i].ip;
				return ip + '&ensp;Launched'; 
			});
		}
	}

	//Draw all nodes and bounding box in dataflow graph
    for(var j = 0;j<classified_data.length;j++){
        var total_num = cores[j];
        var type_num = types.length;
        radius.push(Math.min(
                    ((drawing_params.dfg_width - drawing_params.pool_margin*2)/total_num - drawing_params.nodes_spacing*2),
                    ((drawing_params.dfg_height - drawing_params.pool_margin)/type_num - drawing_params.layer_margin)/2)
                );
        for(var i = 0;i<type_code.length;i++)
            drawNodesByType(d3.select('#pool_'+j),classified_data,radius[j],type_code[i],total_num,j,i,types,mode);
    }
    drawLinks(links);
    if($('.content').length==0){
    	for(var i = 0;i<type_name.length;i++){
			var content = "<div class='content'><div class='valign'><div class='type-btn'></div><p>"+type_name[i].replace(' ', '<br/>')+"</p></div></div>";
			$('#legend-wrapper-l').append(content);
		}
		$('.content').css('height',(60/type_name.length)+'vh');
		$('.content').each(function(i,e){
			console.log('executed');
			$(e).css('top',5+i*(60/type_name.length)+'vh');
		});
		$('.type-btn').each(function(i,e){
			$(e).css('background-color',msu_color[i]);
		});
		d3.selectAll('.type-btn').on('mouseover',function(d,i){
			d3.selectAll('.node_'+type_code[i]).attr('r',function(){
				var this_class = d3.select(this).attr('class');
				var rt_ind = this_class.substring(this_class.indexOf('node_rt_') + 8);
				console.log("Hover in rt index:"+rt_ind);
				return 1.1*radius[rt_ind];
			});
		});
		d3.selectAll('.type-btn').on('mouseout',function(d,i){
			d3.selectAll('.node_'+type_code[i]).attr('r',function(){
				var this_class = d3.select(this).attr('class');
				var rt_ind = this_class.substring(this_class.indexOf('node_rt_') + 8);
				console.log("Hover out rt index:"+rt_ind);
				return radius[rt_ind];
			});
		});
    }
});

socket.on('launching',function(data){
	var filename = data;
	d3.selectAll('.ip_n_status').html(function(){
		var str = d3.select(this).html().toString();
		var ip = getIpAddress(str);
		return ip + '&ensp;Launching'; 
	});
	d3.select('#status_board').select('rect')
		.transition()
		.duration(500)
		.attr('fill',status_color['running']);
	d3.select('#status_board').select('text')
		.transition()
		.duration(500)
		.text('Running    '+filename);
});

socket.on('launched',function(data){
	var index = data;
	d3.select('#ip_n_status_'+index).html(function(){
		var str = d3.select(this).html().toString();
		var ip = getIpAddress(str);
		return ip + '&ensp;Launched'; 
	});
	launched_num++;
	console.log(launched_num);
	if(launched_num == rt_num){
		state_flag = 3;
		socket.emit('all-launched','placeholder');
	}
});

socket.on('stop-finish',function(data){
	var index = data;
	console.log('STOP-SIGNAL:'+data);

    var selection;
    if (index > 0) {
        selection = d3.select("#ip_n_status_" + index);
        launched_num--;
    } else {
        selection = d3.selectAll(".ip_n_status")
        launched_num = 0;
    }
    selection.html(function(){
        var str = d3.select(this).html().toString();
        var ip = getIpAddress(str);
        return ip + '&ensp;Idle';
    });
	console.log("Remaining:"+launched_num);
    // Index == -1 forces a reset
	if(launched_num <= 0 ){
		state_flag = 1;
		d3.select('#status_board').select('rect')
			.transition()
			.duration(500)
			.attr('fill',status_color['idle']);
		d3.select('#status_board').select('text')
			.transition()
			.duration(500)
			.text('Idle    '+filename);
		$('.attack-board').remove();
		socket.emit('stop-finish','placeholder')
	}
});

socket.on('begin starting',function(data){
	state_flag = 2;
	var filename = data
	d3.select('#status_board').select('rect')
		.transition()
		.duration(500)
		.attr('fill',status_color['running']);
	d3.select('#status_board').select('text')
		.transition()
		.duration(500)
		.text('Running    '+filename);
});

socket.on("disconnect", function(){
    console.log("client disconnected from server");
    d3.select('#status_board').select('rect')
			.transition().duration(1000)
			.attr('fill','#ff6666')
			.transition().duration(1000)
			.attr('fill','#ff3333')
			.transition().duration(1000)
			.attr('fill','#ff6666');
    disc_timer = setInterval(function(){
		d3.select('#status_board').select('rect')
			.transition().duration(1000)
			.attr('fill','#ff6666')
			.transition().duration(1000)
			.attr('fill','#ff3333')
			.transition().duration(1000)
			.attr('fill','#ff6666');
    },3000);
    d3.select('#status_board').select('text')
		.transition()
		.duration(500)
		.text('Disconnect');
});

