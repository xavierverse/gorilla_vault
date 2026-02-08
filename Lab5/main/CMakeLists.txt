idf_component_register(SRCS "station_example_main.c"
                    INCLUDE_DIRS "."
					EMBED_TXTFILES server_root_cert.pem local_server_cert.pem)
