module main

import os
import ttytm.vibe
import rand
import db.sqlite
import sync.pool

enum ResultType {
	success
	error
}

struct ParseResult {
	status ResultType
	data   string
}

struct Cache {
	key        string
	resolution ?string
	value      string
}

pub struct SResult {
	success bool
	data    string
}

fn main() {
	args := arguments()

	if args.len < 2 {
		println('Argument for uri missing !')
		return
	} else if !args[1].starts_with('http') || args[1] == 'http' {
		println('Uri is expected to be a http URL !')
		return
	}
	uri := args[1].trim_space()
	mut folder_name := ''
	mut video_uri := ''

	existing := get_cache(uri)

	if existing.key != 'err' {
		folder_name = existing.value
		video_uri = existing.resolution or { '' }
		println('Found existing cache\nArgument for name will be ignored\n')
	} else if args.len == 3 {
		folder_name = args[2]
		add_cache(uri, folder_name)!
	} else {
		folder_name = random_string(15)
		add_cache(uri, folder_name)!
	}

	resp := vibe.get(uri) or {
		println('failed to fetch data from the server')
		return
	}

	if video_uri.len <= 0 {
		resolution := parse_manifest(resp.body)
		if resolution.status == ResultType.error {
			if check_if_video_manifest(resp.body.trim_space()) {
				println('Found video manifest')
				video_uri = uri
			}
		} else {
			video_uri = make_url(resolution.data, uri)
		}

		update_cache(uri, video_uri)!
	}

	if uri == video_uri {
		start_download(resp.body.trim_space(), uri, folder_name)
	} else {
		video_uri_resp := vibe.get(video_uri) or {
			println('failed to fetch data from the server')
			return
		}
		start_download(video_uri_resp.body.trim_space(), video_uri, folder_name)
	}
	merge_status := combine_video(folder_name)
	if merge_status == ResultType.success {
		rm_key_cache(uri)!
		os.rmdir_all(folder_name)!
	}
}

fn update_merge_file(folder_path string, content string) {
	mut file := os.open_file(folder_path, 'a') or { return }
	file.writeln(content) or { return }
	file.close()
}

fn combine_video(folder_name string) ResultType {
	user_home := os.home_dir()
	downloads_folder := os.join_path(user_home, 'Downloads')

	cmd := 'ffmpeg -version'
	locally_installed := os.execute(cmd)
	mut success := 90

	output_file := "${downloads_folder}/${folder_name}.mp4"

	if os.exists(output_file) {
		println("\033[41m File ${output_file} already exists \033[0m")
		return ResultType.error
	}

	ffmpeg_cmd := '-f concat -safe 0 -i ./${folder_name}/merge_file.txt -c copy ${output_file}'

	if locally_installed.exit_code == 0 {
		println('\033[42m Using local install of ffmpeg to merge video \033[0m')
		res := os.execute('ffmpeg ${ffmpeg_cmd}')
		success = res.exit_code
	} else {
		println("\033[43m Can't find ffmpeg installed on local machine \033[0m")
		println('\033[43m Using statically compiled binary \033[0m')
		res := os.execute('./ffmpeg ${ffmpeg_cmd}')
		success = res.exit_code
	}
	if success == 0 {
		println('\n\033[42m Success: video saved at ${output_file} \033[0m')
		return .success
	} else {
		return .error
	}
}

fn cache_init() {
	db_exists := os.exists('./cache.db')
	if !db_exists {
		mut db := sqlite.connect('./cache.db') or {
			println('db initialization failed !')
			return
		}
		sql db {
			create table Cache
		} or {
			println('db initialization failed !')
			return
		}
		db.close() or {
			println('db initialization failed !')
			return
		}
	}
}

fn update_cache(key string, value string) ! {
	mut db := sqlite.connect('./cache.db')!
	sql db {
		update Cache set resolution = value where key == key
	}!

	db.close()!
}

fn rm_key_cache(key string) ! {
	cache_init()
	mut db := sqlite.connect('./cache.db')!
	db.exec_param('DELETE FROM Cache WHERE key = ? ', key)!

	db.close()!
}

fn add_cache(key string, value string) ! {
	cache_init()
	mut db := sqlite.connect('./cache.db')!
	item := Cache{
		key:   key
		value: value
	}

	sql db {
		insert item into Cache
	}!
	db.close()!
}

fn get_cache(key string) Cache {
	cache_init()
	mut db := sqlite.connect('./cache.db') or {
		return Cache{
			key:   'err'
			value: 'error connect to cache db'
		}
	}
	res := sql db {
		select from Cache where key == key
	} or { return Cache{
		key:   'err'
		value: 'key not found'
	} }
	db.close() or { return Cache{
		key:   'err'
		value: 'error closing connection'
	} }
	if res.len == 0 {
		return Cache{
			key:   'err'
			value: 'key not found'
		}
	}
	return res[0]
}

fn random_string(limit int) string {
	char_set := 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
	mut r_string := ''
	mut i := 0
	for i < limit {
		r_string += rand.element(char_set.split('')) or { return '' }
		i++
	}
	return r_string
}

pub struct DownloadsArr {
	url     string
	segment string
	folder  string
}

fn start_download(response_body string, url string, folder_name string) {
	mut seperated_lines := response_body.split('\n').map(it.trim_space())
	seperated_lines = seperated_lines.filter(!it.starts_with('#') && it.len > 0)

	if !os.exists(folder_name) {
		os.mkdir(folder_name) or { return }
	}

	mut spro := []DownloadsArr{}

	for line in seperated_lines {
		segment_name := make_segment(line)
		complete_uri := make_url(line, url)
		spro.insert(spro.len, DownloadsArr{
			url:     complete_uri
			segment: segment_name
			folder:  folder_name
		})
	}

	mut pp := pool.new_pool_processor(callback: download_segment)
	pp.work_on_items(spro)
	for i, x in pp.get_results[SResult]() {
		total := seperated_lines.len - 1
		progress := int(f64(i) / f64(total) * 100)

		if x.success == true {
			print('\rDownloading ${progress}%  ${i}/${total} ')
			os.flush()
			update_merge_file('./${folder_name}/merge_file.txt', "file '${x.data}'")
		} else {
			print("\r${x.data}")
			os.flush()
		}
	}
	print('\n')
	println("\033[43m Download complete \033[0m")
}


fn download_segment(mut pp pool.PoolProcessor, idx int, wid int) &SResult {
	item := pp.get_item[DownloadsArr](idx)
	segment_name := make_segment(item.segment)

	folder := item.folder

	if os.exists('./${folder}/${segment_name}') && os.file_size('./${folder}/${segment_name}') != 0 {
		return &SResult{
			success: false
			data:    '\033[42m\r Cache Hit \033[0m'
		}
	}

	vibe.download_file(item.url, './${folder}/${segment_name}') or {
		println(err)
		return &SResult{
			success: false
			data:    'Download failed'
		}
	}

	return &SResult{
		success: true
		data:    segment_name
	}
}

fn make_url(segment string, original_url string) string {
	mut url := original_url.split('/')
	url.pop()
	if segment.starts_with('http') {
		return segment
	} else {
		return url.join('/') + '/' + segment
	}
}

fn make_segment(segment string) string {
	if !segment.starts_with('http') && !segment.contains("/") {
		return segment
	} else {
		mut url := segment.split('/')
		return url.pop()
	}
}

fn check_if_video_manifest(manifest string) bool {
	mut separated := manifest.split('\n').map(it.trim_space())
	separated = separated.filter(!it.starts_with('#') && it.len > 1)
	if separated.any(!it.contains('.m3u8')) {
		return true
	}

	return false
}

fn parse_manifest(manifest string) ParseResult {
	mut separated := manifest.split('\n').map(it.trim_space())
	mut options := map[string]string{}

	for i, line in separated {
		if line.starts_with('#EXT-X-STREAM-INF') && line.contains('RESOLUTION') {
			mut resolution := line.split(',')
			resolution = resolution.filter(it.starts_with('RESOLUTION='))

			if resolution.len > 0 {
				if i + 1 < line.len && separated[i + 1].str().ends_with('.m3u8') {
					options[resolution[0]] = separated[i + 1].str()
				}
			}
		}
	}

	if options.len == 0 {
		return ParseResult{
			status: .error
			data:   'No resolution found.'
		}
	}

	for i, option in options.keys() {
		println((i + 1).str() + ') ' + option)
	}

	mut selected := os.input('Select the resolution : ').int()

	for selected < 1 || selected > options.len {
		println('Try again')
		selected = os.input('Select the resolution : ').int()
	}

	return ParseResult{
		status: .success
		data:   options.values()[selected - 1]
	}
}
