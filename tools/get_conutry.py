import datetime

def get_country_code():
    global MORROR
    if MORROR == 0:
        try:
            offset = datetime.datetime.now().astimezone().utcoffset()
            if offset is not None and int(offset.total_seconds()) == 8 * 3600:
                MORROR = 1
            else:
                MORROR = 2
        except Exception:
            MORROR = 2

    print(MORROR)


if __name__ == "__main__":
    MORROR = 0
    get_country_code()
