from flask import Flask, request, jsonify, make_response
from pymongo import MongoClient
import base64
import os

from datetime import timedelta
from flask_jwt_extended import JWTManager, create_access_token, jwt_required
from flask_jwt_extended import get_jwt_identity, unset_jwt_cookies

app = Flask(__name__)

# Connect to MongoDB
client = MongoClient("mongodb+srv://gserver:HarrisonDaGoat@cluster0.6xdx27g.mongodb.net/")
db = client["gserver"]
users_collection = db["gOwners"]

# Secret key for JWT encoding and decoding
app.config['SECRET_KEY'] = 'cse_capstone'
#app.config['SECRET_KEY'] = os.urandom(24)
#app.config['JWT_ACCESS_TOKEN_EXPIRES'] = timedelta(minutes=5)

jwt = JWTManager(app)

#/signup (POST):
#Name: User Registration
#Description: Allows new users to register by providing personal information and credentials,
#             such as username, email, and password. It checks for duplicate usernames or emails.
@app.route('/signup', methods=['POST'])
def signup():
    # Extract data from the POST request
    data = request.json
    username = data.get('username')
    email = data.get('email')
    password = data.get('password')
    vault_number = data.get('vault_number')
    vault_code = data.get('vault_code')

    # Ensure all fields are provided
    if not all([username, email, password, vault_number, vault_code]):
        return jsonify({'message': 'Missing fields'}), 400
    
    # Check if the username or email already exists in the database
    if users_collection.find_one({"$or": [{"username": username}, {"email": email}]}):
        return jsonify({'message': 'Username or email already exists'}), 400

    user = {
        "username": username,
        "email": email,
        "password": password,
        "vault_number": vault_number,
        "vault_code": vault_code,
        "motion_status": "false",
        "frames": "NONE",
        "packages": {},
        "blacklist": []
    }

    # Insert the user document into the database
    users_collection.insert_one(user)

    return jsonify({"message": "User created successfully."}), 201

#/login (POST):
#Name: User Login
#Description: Authenticates users based on email or username and password, returning a JWT access token if credentials are valid.
@app.route('/login', methods=['POST'])
def login():
    # Extract data from the POST request
    data = request.json
    login = data.get('login')  # Can be either a username or email
    password = data.get('password')

    if not login or not password:
        return jsonify({'message': 'Missing login or password'}), 400
    
    # Check if the login exists in the database (can be either username or email)
    user = users_collection.find_one({"$or": [{"username": login}, {"email": login}]})
    
    if user and user['password'] == password:
        # Generate JWT token
        username = user.get('username')
        token = create_access_token(identity=username)
        return jsonify({'token': token}), 200
    else:
        return jsonify({'message': 'Invalid login or password'}), 401

#/generate-api-key (POST):
#Name: Generate API Key
#Description: Generates a new 128 bit API key for the logged-in user . Requires JWT authentication.
@app.route('/generate_api_key', methods=['POST'])
@jwt_required()
def generate_api_key():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401

    vault_number = user['vault_number']
    
    # Generate API key based on the vault_number (example logic)
    api_key = base64.b64encode(vault_number.encode("utf-8")).decode("utf-8")

    # Return the generated API key in a JSON response
    return jsonify({'api_key': api_key}), 200

#/vault_code (GET):
#Name: Retrieve Vault Code  || Authorization : Bearer <api_key>
#Description: Retrieves the vault code associated with the user identified by the API key provided in the  header.
@app.route('/vault_code', methods=['GET'])
def get_vault_code():
    # Get the API key from the Authorization header
    auth_header = request.headers['Authorization']

    if auth_header and auth_header.startswith('Bearer '):
        api_key = auth_header.split(' ')[1]
    
    if not api_key:
        return jsonify({"message": "Missing API key"}), 401

    decoded_string = base64.b64decode(api_key)
    vault_number = decoded_string.decode("utf-8")
    
    # Check if the API key exists in the database
    user = users_collection.find_one({"vault_number": vault_number})

    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    # Extract and return the vault_code
    vault_code = user["vault_code"] # Assuming there's only one vault per user
    #return jsonify({"vault_code": vault_code}), 200
    return vault_code, 200

#/vault_code(PUT):
#Name: Update Vault Code   || Authorization : Bearer <access_token>
#Description: Updates the vault code for the logged-in user. Requires JWT authentication and checks for a provided vault code.
@app.route('/vault_code', methods=['PUT'])
@jwt_required()
def update_vault_code():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401
    
    data = request.json
    new_vault_code = data.get('vault_code')
    if not new_vault_code:
        return jsonify({"message": "New vault code not provided"}), 400
    
    result = users_collection.update_one(
        {"username": username},
        {"$set": {"vault_code": new_vault_code}}
    )

    # Return updated user data
    return jsonify({"message": "Vault code updated successfully", "vault_code": new_vault_code}), 200

#/packages (POST):
#Name: Add Package  || Authorization : Bearer <access_token>
#Description: Adds a new package record. It checks for required fields and duplicate package IDs. Requires JWT authentication.
@app.route('/packages', methods=['POST'])
@jwt_required()
def add_package():
    # Extract JWT token from the Authorization header
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401

    data = request.json
    package_id = data.get('package_id')
    if not package_id:
        return jsonify({"message": "Package ID not provided"}), 400
    
    # Update the user document in the database
    result = users_collection.update_one(
        {"username": username},
        {"$set": {f"packages.{package_id}": "false"}}
    )

    if result.modified_count == 0:
        return jsonify({'message': 'Failed to add package'}), 500

    # Return updated user data
    return jsonify({"message": "Package added successfully"}), 200

#/packages (GET):
#Name: List Packages   || Authorization : Bearer <access_token>
#Description: Retrieves all package records from the database. Requires JWT authentication.
@app.route('/packages', methods=['GET'])
@jwt_required()
def get_packages():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401

    packages = user.get('packages', {})
    return jsonify({'packages': packages}), 200
    
#/delivery_status (GET):
#Name: Get Delivery Status  || Authorization : Bearer <access_token>
#Description: Fetches the delivery status of a package associated with the user . Requires JWT authentication.
@app.route('/delivery_status', methods=['GET'])
@jwt_required()
def get_delivery_status():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401
    
    package_id = request.args.get('package_id')
    if not package_id:
        return jsonify({"message": "Package ID not provided"}), 400
    
    # Check if the package ID exists in the user's Packages dictionary
    if package_id not in user.get('packages', {}):
        return jsonify({'message': 'No such Package ID exists'}), 404

    # Extract the delivery status from the user's Packages dictionary
    delivery_status = user['packages'][package_id]
    return jsonify({'package_id': package_id, 'delivery_status': delivery_status}), 200
    
#/delivery_status (PUT):
#Name: Update Delivery Status  || Authorization : Bearer <api_key>
#Description: Updates the delivery status of a specific package identified by the package ID.
@app.route('/delivery_status', methods=['PUT'])
def update_delivery_status():
    # Get the API key from the Authorization header
    auth_header = request.headers['Authorization']

    if auth_header and auth_header.startswith('Bearer '):
        api_key = auth_header.split(' ')[1]
    
    if not api_key:
        return jsonify({"message": "Missing API key"}), 401

    decoded_string = base64.b64decode(api_key)
    vault_number = decoded_string.decode("utf-8")
    
    # Check if the API key exists in the database
    user = users_collection.find_one({"vault_number": vault_number})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    data = request.json
    package_id = data.get('package_id')
    delivery_status = data.get('delivery_status')
    if not package_id:
        return jsonify({"message": "Package ID not provided"}), 400
    
    delivery_status = data.get('delivery_status')
    if not delivery_status:
        return jsonify({"message": "Delivery status not provided"}), 400
    
    if delivery_status not in ['true', 'false']:
        return jsonify({"message": "Invalid delivery status provided"}), 400
    
    # Check if the package ID exists in the user's Packages dictionary
    if package_id not in user.get('packages', {}):
        return jsonify({'message': 'No such Package ID exists'}), 404

    # Update the user document in the database
    result = users_collection.update_one(
        {"vault_number": vault_number},
        {"$set": {f"packages.{package_id}": delivery_status}}
    )

    if result.modified_count == 0:
        return jsonify({'message': 'Failed to update delivery status'}), 500

    # Return updated user data
    return jsonify({"message": "Delivery status updated successfully"}), 200

#/undelivered_packages (GET):
#Name: List Undelivered Packages   || Authorization : Bearer <api_key>
#Description: Retrieves a list of all packages that have not yet been delivered, based on the API key provided in the header.
@app.route('/undelivered_packages', methods=['GET'])
def get_undelivered_packages():
    # Get the API key from the Authorization header
    auth_header = request.headers['Authorization']

    if auth_header and auth_header.startswith('Bearer '):
        api_key = auth_header.split(' ')[1]
    
    if not api_key:
        return jsonify({"message": "Missing API key"}), 401

    decoded_string = base64.b64decode(api_key)
    vault_number = decoded_string.decode("utf-8")
    
    # Check if the API key exists in the database
    user = users_collection.find_one({"vault_number": vault_number})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    #undelivered_packages = {package_id: status for package_id, status in user.get('packages', {}).items() if status.lower() == 'false'}
    undelivered_packages = [package_id for package_id, status in user.get('packages', {}).items() if status.lower() == 'false']

    # Return the filtered undelivered packages
    #return jsonify(undelivered_packages), 200
    return undelivered_packages, 200

#/motion (PUT):
#Name: Update Motion Status  || Authorization : Bearer <api_key>
#Description: Updates the motion status for a user identified by the API key provided in the header. The motion status must be either True or False.
@app.route('/motion', methods=['PUT'])
def update_motion_status():
    # Get the API key from the Authorization header
    auth_header = request.headers['Authorization']

    if auth_header and auth_header.startswith('Bearer '):
        api_key = auth_header.split(' ')[1]
    
    if not api_key:
        return jsonify({"message": "Missing API key"}), 401

    decoded_string = base64.b64decode(api_key)
    vault_number = decoded_string.decode("utf-8")
    
    # Check if the API key exists in the database
    user = users_collection.find_one({"vault_number": vault_number})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    data = request.json
    motion_status = data.get('motion_status')
    if not motion_status:
        return jsonify({"message": "Motion status not provided"}), 400
    
    
    if motion_status not in ['true', 'false']:
        return jsonify({"message": "Invalid motion status provided"}), 400

    # Update the user document in the database
    result = users_collection.update_one(
        {"vault_number": vault_number},
        {"$set": {"motion_status": motion_status}}
    )

    if result.modified_count == 0:
        return jsonify({'message': 'Failed to update motion status'}), 500

    # Return updated user data
    return jsonify({"message": "Motion status updated successfully"}), 200
    
#/motion (GET):
#Name: Get Motion Status  || Authorization : Bearer <access_token>
#Description: Retrieves the motion status associated with the logged-in user. Requires JWT authentication.
@app.route('/motion', methods=['GET'])
@jwt_required()
def get_motion_status():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401

    motion_status = user["motion_status"] 
    return jsonify({"motion_status": motion_status}), 200
    
#/camera (POST):
#Name: Upload Image  || Authorization : Bearer <api_key>
#Description: Streams video
@app.route('/camera', methods=['POST'])
def stream():
    # Get the API key from the Authorization header
    auth_header = request.headers['Authorization']

    if auth_header and auth_header.startswith('Bearer '):
        api_key = auth_header.split(' ')[1]
    
    if not api_key:
        return jsonify({"message": "Missing API key"}), 401

    decoded_string = base64.b64decode(api_key)
    vault_number = decoded_string.decode("utf-8")
    
    # Check if the API key exists in the database
    user = users_collection.find_one({"vault_number": vault_number})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    data = request.json
    frame = data.get('frame')
    if not frame:
        return jsonify({"message": "Frame not provided"}), 400

    # Update the user document in the database
    result = users_collection.update_one(
        {"vault_number": vault_number},
        {"$set": {"frames": frame}}
    )

    if result.modified_count == 0:
        return jsonify({'message': 'Failed to update frame'}), 500

    # Return updated user data
    return jsonify({"message": "Frame updated successfully"}), 200

#/camera (GET):
#Name: Retrieve Image  || Authorization : Bearer <access_token>
#Description: Returns an image associated with the logged-in user. Requires JWT authentication.
@app.route('/camera', methods=['GET'])
@jwt_required()
def get_stream():
    # Extract the username from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401
    
    if request.headers.get('Authorization') in user['blacklist']:
        return jsonify({'message': 'Token has been invalidated'}), 401

    frame = user["frames"]
    return jsonify({"frame": frame}), 200
    
#/logout (POST):
#Name: User Logout  || Authorization : Bearer <access_token>
#Description: Logs out the user by invalidating the JWT token. Requires JWT authentication.
@app.route('/logout', methods=['POST'])
@jwt_required()  # Require a valid token for logout
def logout():
    # Get the identity from the JWT token
    username = get_jwt_identity()

    user = users_collection.find_one({"username": username})
    if not user:
        return jsonify({"message": "Unauthorized"}), 401

    # Unset JWT cookies in the response
    resp = jsonify({'message': 'Logout successful'})
    unset_jwt_cookies(resp)

    # Add the token to the blacklist
    users_collection.update_one(
        {"username": username},
        {"$push": {"blacklist": request.headers.get('Authorization')}}
    )

    return resp, 200

if __name__ == '__main__':
    #app.run(debug=True, host='0.0.0.0', port=)
    app.run(debug=True)
