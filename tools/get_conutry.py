import requests
import json

def get_country_code():
    global MORROR
    if MORROR == 0:
        try:
            response = requests.get('http://www.ip-api.com/json', timeout=5)
            response.raise_for_status()  
            # print(response.elapsed)
            result = response.json()
            # print(result)

            country = result.get('country', '')
            # print("country: ", country)
            if country == "China":
                MORROR = 1
            else:
                MORROR = 2
        except requests.exceptions.RequestException as e:
            # print(f"curl http://www.ip-api.com/json failed ... Error: {e}")
            MORROR = 1
    
    print(MORROR)


if __name__ == "__main__":
    MORROR = 0
    get_country_code()